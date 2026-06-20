/*
 * boot32.c - FAT32 Boot Sector parameter mapping (C17 standard)
 * Original assembly source: boot32.asm
 */

#include <stdint.h>

struct fat32_boot_layout {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t max_root_entries;
    uint16_t total_sectors_short;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_legacy;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    
    /* FAT32 Specific fields */
    uint32_t sectors_per_fat_32;
    uint16_t extended_flags;
    uint16_t filesystem_version;
    uint32_t root_cluster_num;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
};
