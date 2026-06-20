/*
 * boot.c - MBR Boot Sector parameter mapping (C17 standard)
 * Original assembly source: boot.asm
 */

#include <stdint.h>

struct boot_sector_layout {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t max_root_entries;
    uint16_t total_sectors_short;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    /* ... Extended parameters ... */
};
