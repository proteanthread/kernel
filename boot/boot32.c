/*
 * boot32.c - FAT32 Boot Sector parameter mapping (C17 standard)
 *
 * Architectural Role:
 *   Specifies the structure of the FAT32 Boot Sector. Matches the BPB and EBPB fields
 *   queried by boot32.asm during the single-stage bootloader relocation and kernel load phase.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Field naming and auxiliary validation helpers in C.
 *   - CANNOT BE CHANGED: Layout, sizes, and padding of the fields inside the structure.
 *     Must align exactly with BPB offsets expected by assembly routines and file system builders.
 *
 * Expected Behavior:
 *   - Defines the sector offsets used to relocate the bootloader to segment 0x1FE0 (relocated to 27A00h)
 *     and load directory clusters, the FAT, and KERNEL.SYS to segment 0x0060 (LOADSEG).
 *
 * Diagnostics & Recovery:
 *   - Boot failures due to invalid BPB parameters can be checked by verifying filesystem geometry
 *     via debugging print/beep routines emulated in boot32.asm.
 */

#include <stdint.h>

/* 
 * Memory layout for the LibreDOS FAT32 single stage boot process (copied from boot32.asm):
 *
 *	...
 *	|-------| 1FE0h:7E00h = 27C00h (159 KiB)
 *	|BOOTSEC| loader relocates itself here first thing,
 *	|RELOC.	|  before loading root directory/FAT/kernel file
 *	|-------| 1FE0h:7C00h = 27A00h (158 KiB)
 *	| STACK | below relocated loader, above FAT sector (size 22 KiB)
 *	...
 *	|-------| 2200h:2000h = 24000h (144 KiB)
 *	|  FAT  | (only 1 sector buffered, maximum sector size 8 KiB)
 *	|-------| 2200h:0000h = 22000h (136 KiB)
 *	...
 *	|-------| 0000h:7E00h = 07E00h (31.5 KiB)
 *	|BOOTSEC| overwritten by the kernel, so the
 *	|ORIGIN | bootsector relocates itself up...
 *	|-------| 0000h:7C00h = 07C00h (31 KiB)
 *	...
 *	|-------|
 *	|KERNEL	| maximum size 128 KiB (overwrites bootsec origin)
 *	|LOADED	| (holds 1 sector directory buffer before kernel file load)
 *	|-------| 0060h:0000h = 00600h (1.5 KiB)
 */

struct fat32_boot_layout {
    uint8_t  jmp_boot[3];           // bp-Entry+0x00 Jump instruction to boot code
    uint8_t  oem_name[8];           // bp+0x03       OEM label
    uint16_t bytes_per_sector;      // bp+0x0b       bytes/sector
    uint8_t  sectors_per_cluster;   // bp+0x0d       sectors/allocation unit
    uint16_t reserved_sectors;      // bp+0x0e       # reserved sectors
    uint8_t  num_fats;              // bp+0x10       # of fats
    uint16_t max_root_entries;      // bp+0x11       # of root dir entries (0 for FAT32)
    uint16_t total_sectors_short;   // bp+0x13       # sectors total in image (0 for FAT32)
    uint8_t  media_descriptor;      // bp+0x15       media descrip: fd=2side9sec, etc...
    uint16_t sectors_per_fat_legacy;// bp+0x16       # sectors in a fat (0 for FAT32)
    uint16_t sectors_per_track;     // bp+0x18       # sectors/track
    uint16_t num_heads;             // bp+0x1a       # heads
    uint32_t hidden_sectors;        // bp+0x1c       # hidden sectors
    uint32_t total_sectors_long;    // bp+0x20       # sectors if > 65536
    
    // FAT32 Specific fields
    uint32_t sectors_per_fat_32;    // bp+0x24       Sectors/Fat (dd)
    uint16_t extended_flags;        // bp+0x28       Flags (for fat mirroring)
    uint16_t filesystem_version;    // bp+0x2a       Filesystem version (usually 0)
    uint32_t root_cluster_num;      // bp+0x2c       Starting cluster of root directory
    uint16_t fs_info_sector;        // bp+0x30       Sector number of fs-info sector
    uint16_t backup_boot_sector;    // bp+0x32       Sector number of boot sector backup
    uint8_t  reserved[12];          // bp+0x34       Reserved
    uint8_t  drive_number;          // bp+0x40       Drive number
    uint8_t  reserved_ext;          // bp+0x41       Reserved
    uint8_t  ext_boot_signature;    // bp+0x42       Extended boot signature
    uint32_t volume_id;             // bp+0x43       Volume serial number
    uint8_t  volume_label[11];      // bp+0x47       Volume label
    uint8_t  filesystem_type[8];    // bp+0x52       Filesystem ID ("FAT32")
};
