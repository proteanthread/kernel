/*
 * kernel.c - Anchor tables for List of Lists & SDA (C17 standard)
 * Original assembly source: kernel.asm
 */

#include <stdint.h>

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
