/*
 * kernel.c - Anchor tables for List of Lists & SDA (C17 standard)
 * Part of the LibreDOS Memory Specification (LMS)
 * Original assembly source: kernel.asm
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
