/*
 * LibreDOS Kernel - Anchor Tables for List of Lists, SDA, & Virtual Console Contexts
 *
 * Architectural Role:
 *   Declares the global tracking structures for the 1MB Hybrid Memory Mode virtual consoles
 *   and system instance state contexts (dos_chunk_t). Integrates directly with variables
 *   defined in kernel.asm (such as List of Lists anchors and Swappable Data Area bounds).
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Maximum virtual system limits (MAX_VIRTUAL_SYSTEMS), layout of internal
 *     context tracking maps, and chunk initialization parameters.
 *   - CANNOT BE CHANGED: Struct members mapped to low-level assembly offsets must not be reordered
 *     or have their sizes changed. Struct padding rules must be strictly preserved to prevent
 *     pointer translation misalignment in assembly routines.
 *
 * Expected Behavior:
 *   - Multiple system execution segments are dynamically managed via memory paging. Paging context
 *     switches must guarantee register isolation, keeping each DOS application context in its
 *     own isolated boundaries.
 *
 * Diagnostics & Recovery:
 *   - Misaligned structs lead to memory leaks or data contamination between virtual DOS sessions.
 *     Verify structural offsets via assembly listings (.lst) generated during compilation.
 */

#include <stdint.h>
#include <stdbool.h>

#define MAX_VIRTUAL_SYSTEMS 16

/* Forward declarations matching entry.c structure context */
typedef struct {
    uint8_t *handle_address;
    uint16_t size_pages;
    uint16_t physical_page;
} ems_handle_t;

typedef struct {
    uint8_t *base_address;
    uint32_t chunk_id;
    uint16_t active_console;
    uint16_t umb_start_seg;
    uint16_t umb_size_paras;
    bool     umb_linked;
    struct {
        uint8_t *ptr;
        uint32_t size_bytes;
        bool     in_use;
    } xms_handles[128];
    struct {
        ems_handle_t handles[128];
        uint8_t *page_frame;
        uint8_t *page_mappings[4];
    } ems;
} dos_chunk_t;

/* Global virtual machine instance tracking list (LMS Core) */
dos_chunk_t g_virtual_systems[MAX_VIRTUAL_SYSTEMS];
uint32_t    g_active_system_id = 0;

/* Anchor of internal DOS variables */
struct list_of_lists {
    uint16_t first_mcb_seg;
    uint32_t first_dpb_ptr;
    uint32_t sft_head_ptr;
    uint32_t clock_driver_ptr;
    uint32_t con_driver_ptr;
    uint16_t max_sector_size;
    uint32_t cds_array_ptr;
    uint32_t fcb_table_ptr;
    uint8_t  num_block_devices;
    uint8_t  lastdrive;
} __attribute__((packed));

struct swappable_data_area {
    uint8_t  error_mode;
    uint8_t  indos_flag;
    uint8_t  crit_error_drive;
    uint8_t  error_locus;
    uint16_t error_code;
    uint32_t dta_address;
    uint16_t current_psp;
} __attribute__((packed));

/* Instance definitions */
struct list_of_lists g_list_of_lists;
struct swappable_data_area g_sda;

/* Initializes the LMS virtual memory structures for a guest chunk */
void lms_init_system_chunk(uint32_t system_id, uint8_t *host_base_address) {
    if (system_id >= MAX_VIRTUAL_SYSTEMS) return;
    
    dos_chunk_t *chunk = &g_virtual_systems[system_id];
    chunk->base_address = host_base_address;
    chunk->chunk_id = system_id;
    chunk->active_console = system_id;
    chunk->umb_linked = false;
    chunk->umb_start_seg = 0;
    chunk->umb_size_paras = 0;
    
    /* Zero out handle blocks */
    for (int i = 0; i < 128; i++) {
        chunk->xms_handles[i].in_use = false;
        chunk->xms_handles[i].host_ptr = NULL;
        chunk->xms_handles[i].size_bytes = 0;
        
        chunk->ems.handles[i].allocated = false;
        chunk->ems.handles[i].host_ptr = NULL;
    }
    
    chunk->ems.page_frame = host_base_address + 0xD0000; /* Segment D000h */
    for (int i = 0; i < 4; i++) {
        chunk->ems.page_mappings[i] = NULL;
    }
}
