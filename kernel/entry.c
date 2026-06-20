/*
 * LibreDOS Kernel - System API Interrupt Router & Context Entry
 *
 * Architectural Role:
 *   Routes LIM EMS, XMS, and virtualized system calls. Implements the LibreDOS
 *   Memory Specification (LMS) routing services including the 4KB page frame 
 *   allocator, XMS handler (HIMEM), UMB link emulator, and LIM EMS 4.0 emulator.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Logical conditions inside allocation checks, page tracking arrays,
 *     and handler statistics.
 *   - CANNOT BE CHANGED: Assembly linkage calling conventions, layout of registers in the 
 *     context structs, and hardcoded EMS/XMS segment limits. Alignment of MCB headers 
 *     created for the UMB area must remain paragraph-aligned (16-byte boundary).
 *
 * Expected Behavior:
 *   - Operates under strict real-mode segment limit assumptions. Emulated EMS/XMS memory
 *     calls must avoid memory-overlap and must map physical memory pages dynamically 
 *     within the 1MB DOS memory bounds (typically mapping EMS frames at segment 0xE000).
 *
 * Diagnostics & Recovery:
 *   - Trace memory corruption by verifying segment bases in map files (e.g. kwc38632.map).
 *   - Check stack footprint in registers struct if stack-switching fails.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define CHUNK_SIZE_1MB   1048576
#define XMS_MAX_HANDLES  128
#define EMS_MAX_PAGES    2048
#define EMS_PAGE_SIZE    16384 /* 16KB per page */
#define SYSTEM_PAGE_SIZE 4096  /* 4KB page frame allocator */

/* CPU context structures for real-mode register emulation */
typedef struct {
    uint32_t AX, BX, CX, DX;
    uint32_t SI, DI, BP, SP;
    uint16_t DS, ES, FS, GS, SS, CS;
    uint32_t Flags;
} regs_context_t;

/* LMS Page Frame descriptor (BSD-inspired page allocations) */
typedef struct {
    uint32_t page_id;
    uint32_t flags;
    bool     in_use;
} lms_page_t;

/* LIM EMS 4.0 handle structure mapping allocated pages */
typedef struct {
    uint8_t *host_ptr;      /* Pointer to allocated host memory pages */
    uint16_t page_count;    /* Number of logical 16KB pages */
    uint16_t handle_id;     /* Unique EMS handle number */
    bool     allocated;
} ems_handle_t;

/* LMS 1MB Hybrid Memory Mode context wrapper */
typedef struct {
    uint8_t *base_address;   /* 64-bit host address pointer mapped to conventional memory */
    uint32_t chunk_id;       /* Virtual system / VM context identifier */
    uint16_t active_console; /* Target virtual console */

    /* UMB arena parameters */
    uint16_t umb_start_seg;  /* Segment address of UMB start */
    uint16_t umb_size_paras; /* Paragraph count */
    bool     umb_linked;

    /* eXtended Memory (XMS) handle pool */
    struct {
        uint8_t *host_ptr;
        uint32_t size_bytes;
        bool     in_use;
    } xms_handles[XMS_MAX_HANDLES];

    /* Expanded Memory (EMS) context */
    struct {
        ems_handle_t handles[XMS_MAX_HANDLES];
        uint8_t *page_frame;  /* 64KB Page Frame address offset in host_ptr */
        uint8_t *page_mappings[4]; /* Target pointers for 16KB pages (0-3) */
    } ems;
} dos_chunk_t;

/* Dynamic Physical Page Allocator (BSD-inspired Pager) */
static uint8_t g_host_memory_pool[64 * 1024 * 1024]; /* 64MB Simulated Host Memory Pool */
static lms_page_t g_pages[16384];                     /* 16384 * 4KB = 64MB */

void* lms_allocate_pages(uint32_t num_pages) {
    uint32_t contiguous_count = 0;
    uint32_t start_idx = 0;

    for (uint32_t i = 0; i < 16384; i++) {
        if (!g_pages[i].in_use) {
            if (contiguous_count == 0) {
                start_idx = i;
            }
            contiguous_count++;
            if (contiguous_count == num_pages) {
                for (uint32_t j = start_idx; j < start_idx + num_pages; j++) {
                    g_pages[j].in_use = true;
                }
                return &g_host_memory_pool[start_idx * SYSTEM_PAGE_SIZE];
            }
        } else {
            contiguous_count = 0;
        }
    }
    return NULL; /* Out of Memory */
}

void lms_free_pages(void *ptr, uint32_t num_pages) {
    if (ptr == NULL) return;
    uint32_t offset = (uint8_t*)ptr - g_host_memory_pool;
    uint32_t start_idx = offset / SYSTEM_PAGE_SIZE;

    for (uint32_t i = start_idx; i < start_idx + num_pages; i++) {
        g_pages[i].in_use = false;
    }
}

/* Address Translation helper (16-bit Segment:Offset -> 64-bit flat address) */
inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk) {
    uint32_t linear_offset = ((uint32_t)segment << 4) + offset;
    return chunk->base_address + (linear_offset % CHUNK_SIZE_1MB);
}

/* UMB segment linker */
void lms_umb_link(dos_chunk_t *chunk, uint16_t start_seg, uint16_t size_paras) {
    chunk->umb_start_seg = start_seg;
    chunk->umb_size_paras = size_paras;
    chunk->umb_linked = true;

    /* Write UMB Memory Control Block (MCB) header in segment space */
    uint8_t *mcb_ptr = ld_translate_address(start_seg - 1, 0, chunk);
    mcb_ptr[0] = 0x5A; /* Last block 'Z' signature */
    mcb_ptr[1] = 0x00; /* Free PSP owner low */
    mcb_ptr[2] = 0x00; /* Free PSP owner high */
    
    /* Write block size in paragraphs */
    uint16_t size_avail = size_paras - 1;
    mcb_ptr[3] = size_avail & 0xFF;
    mcb_ptr[4] = (size_avail >> 8) & 0xFF;

    /* Set block name to zeros */
    memset(&mcb_ptr[8], 0, 8);
}

/* Virtual XMS Driver Handler (Int 2Fh AX=4310h) */
void lms_xms_service(regs_context_t *regs, dos_chunk_t *chunk) {
    uint8_t func = (regs->AX) & 0xFF;
    
    switch (func) {
        case 0x08: /* Query Free Extended Memory */
            /* Return 32MB free, largest block size */
            regs->AX = 32768; /* KB */
            regs->DX = 32768;
            break;
            
        case 0x09: { /* Allocate Extended Memory Block (EMB) */
            uint32_t size_kb = regs->DX;
            uint32_t pages_needed = (size_kb * 1024 + SYSTEM_PAGE_SIZE - 1) / SYSTEM_PAGE_SIZE;
            void *block = lms_allocate_pages(pages_needed);
            
            if (block) {
                /* Find free XMS handle */
                for (int i = 1; i < XMS_MAX_HANDLES; i++) {
                    if (!chunk->xms_handles[i].in_use) {
                        chunk->xms_handles[i].host_ptr = (uint8_t*)block;
                        chunk->xms_handles[i].size_bytes = size_kb * 1024;
                        chunk->xms_handles[i].in_use = true;
                        regs->AX = 0x0001; /* Success */
                        regs->DX = i;      /* Handle number */
                        return;
                    }
                }
                lms_free_pages(block, pages_needed);
            }
            regs->AX = 0x0000; /* Fail */
            regs->BX = 0x88;   /* Out of memory / all handles in use */
            break;
        }

        case 0x0A: { /* Free Extended Memory Block */
            uint16_t handle = regs->DX;
            if (handle > 0 && handle < XMS_MAX_HANDLES && chunk->xms_handles[handle].in_use) {
                uint32_t size = chunk->xms_handles[handle].size_bytes;
                uint32_t pages = (size + SYSTEM_PAGE_SIZE - 1) / SYSTEM_PAGE_SIZE;
                lms_free_pages(chunk->xms_handles[handle].host_ptr, pages);
                chunk->xms_handles[handle].in_use = false;
                regs->AX = 0x0001; /* Success */
            } else {
                regs->AX = 0x0000; /* Fail */
                regs->BX = 0xA2;   /* Invalid handle */
            }
            break;
        }

        case 0x0B: { /* Move Extended Memory Block */
            /* Src/Dest block structure copy */
            struct xms_move {
                uint32_t length;
                uint16_t src_handle;
                uint32_t src_offset;
                uint16_t dest_handle;
                uint32_t dest_offset;
            } *param = (struct xms_move*)ld_translate_address(regs->DS, regs->SI, chunk);

            uint8_t *src_ptr = NULL;
            uint8_t *dest_ptr = NULL;

            if (param->src_handle == 0) {
                /* Conventional memory pointer translation */
                src_ptr = (uint8_t*)ld_translate_address((param->src_offset >> 16) & 0xFFFF, param->src_offset & 0xFFFF, chunk);
            } else if (param->src_handle < XMS_MAX_HANDLES && chunk->xms_handles[param->src_handle].in_use) {
                src_ptr = chunk->xms_handles[param->src_handle].host_ptr + param->src_offset;
            }

            if (param->dest_handle == 0) {
                dest_ptr = (uint8_t*)ld_translate_address((param->dest_offset >> 16) & 0xFFFF, param->dest_offset & 0xFFFF, chunk);
            } else if (param->dest_handle < XMS_MAX_HANDLES && chunk->xms_handles[param->dest_handle].in_use) {
                dest_ptr = chunk->xms_handles[param->dest_handle].host_ptr + param->dest_offset;
            }

            if (src_ptr && dest_ptr) {
                memcpy(dest_ptr, src_ptr, param->length);
                regs->AX = 0x0001; /* Success */
            } else {
                regs->AX = 0x0000; /* Fail */
                regs->BX = 0xA3;   /* Invalid offset or handle */
            }
            break;
        }

        default:
            break;
    }
}

/* Virtual LIM EMS 4.0 Service Router (Int 67h) */
void lms_ems_service(regs_context_t *regs, dos_chunk_t *chunk) {
    uint8_t func = (regs->AX >> 8) & 0xFF;

    switch (func) {
        case 0x40: /* Get Status */
            regs->AX = 0x0000; /* Success status */
            break;

        case 0x41: /* Get Page Frame segment address */
            regs->AX = 0x0000; /* Success status */
            regs->BX = 0xD000; /* Frame segment segment (D000:0000) */
            break;

        case 0x42: /* Get number of unallocated/total EMS pages */
            regs->AX = 0x0000;
            regs->BX = 1024;   /* Free pages available */
            regs->DX = 2048;   /* Total pages in system */
            break;

        case 0x43: { /* Allocate Handle and Pages */
            uint16_t pages = regs->BX;
            for (int i = 1; i < XMS_MAX_HANDLES; i++) {
                if (!chunk->ems.handles[i].allocated) {
                    uint32_t mem_bytes = pages * EMS_PAGE_SIZE;
                    uint32_t sys_pages = (mem_bytes + SYSTEM_PAGE_SIZE - 1) / SYSTEM_PAGE_SIZE;
                    void *block = lms_allocate_pages(sys_pages);
                    
                    if (block) {
                        chunk->ems.handles[i].host_ptr = (uint8_t*)block;
                        chunk->ems.handles[i].page_count = pages;
                        chunk->ems.handles[i].handle_id = i;
                        chunk->ems.handles[i].allocated = true;
                        
                        regs->AX = 0x0000; /* Success */
                        regs->DX = i;      /* Handle id */
                        return;
                    }
                    regs->AX = 0x8800; /* Out of physical pages */
                    return;
                }
            }
            regs->AX = 0x8500; /* All handles in use */
            break;
        }

        case 0x44: { /* Map Handle Page to Physical Page Frame Window */
            uint16_t handle = regs->DX;
            uint16_t phys_page = regs->AL;
            uint16_t log_page = regs->BX;

            if (handle > 0 && handle < XMS_MAX_HANDLES && chunk->ems.handles[handle].allocated && phys_page < 4) {
                if (log_page < chunk->ems.handles[handle].page_count) {
                    /* Dynamic bank mapping assignment */
                    uint8_t *mapped_addr = chunk->ems.handles[handle].host_ptr + (log_page * EMS_PAGE_SIZE);
                    chunk->ems.page_mappings[phys_page] = mapped_addr;

                    /* Reflect changes in conventional memory mapping offsets */
                    uint8_t *dest_frame = chunk->base_address + (0xD0000 + phys_page * EMS_PAGE_SIZE);
                    memcpy(dest_frame, mapped_addr, EMS_PAGE_SIZE);

                    regs->AX = 0x0000; /* Success */
                } else {
                    regs->AX = 0x8A00; /* Logical page out of range */
                }
            } else {
                regs->AX = 0x8900; /* Invalid handle or page */
            }
            break;
        }

        case 0x45: { /* Deallocate Handle */
            uint16_t handle = regs->DX;
            if (handle > 0 && handle < XMS_MAX_HANDLES && chunk->ems.handles[handle].allocated) {
                uint32_t bytes = chunk->ems.handles[handle].page_count * EMS_PAGE_SIZE;
                uint32_t sys_pages = (bytes + SYSTEM_PAGE_SIZE - 1) / SYSTEM_PAGE_SIZE;
                lms_free_pages(chunk->ems.handles[handle].host_ptr, sys_pages);
                chunk->ems.handles[handle].allocated = false;
                regs->AX = 0x0000; /* Success */
            } else {
                regs->AX = 0x8900; /* Invalid handle */
            }
            break;
        }

        default:
            regs->AX = 0x8400; /* Unknown function code */
            break;
    }
}

/* Routes system call interrupts */
void ld_int21_service(regs_context_t *regs, dos_chunk_t *chunk) {
    uint8_t ah = (regs->AX >> 8) & 0xFF;
    switch (ah) {
        case 0x4C: /* Exit */
            break;
        case 0x58: { /* UMB Link/Allocation Strategy commands */
            uint8_t al = regs->AX & 0xFF;
            if (al == 0x02) { /* Get UMB link status */
                regs->AX = chunk->umb_linked ? 0x0001 : 0x0000;
            } else if (al == 0x03) { /* Link/Unlink UMB space */
                lms_umb_link(chunk, 0xC800, 0x1800); /* Map C800h-DFFFh */
                regs->AX = 0x0001;
            }
            break;
        }
        default:
            break;
    }
}
