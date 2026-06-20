/*
 * LibreDOS Kernel - MBR Boot Sector parameter mapping (C17 standard)
 *
 * Architectural Role:
 *   Specifies the structure of the FAT12/FAT16 MBR boot sector. Matches the BPB and
 *   EBPB fields queried by boot.asm during the bootloader relocation and kernel load phase.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Field naming and auxiliary validation helpers in C.
 *   - CANNOT BE CHANGED: Layout, sizes, and padding of the fields inside the structure.
 *     Must align exactly with BPB offsets expected by assembly routines and file system builders.
 *
 * Expected Behavior:
 *   - Defines the sector offsets used to relocate the bootloader to segment 0x1FE0 (relocated to 27A00h)
 *     and load directory tables, FATs, and KERNEL.SYS to segment 0x0060 (LOADSEG).
 *
 * Diagnostics & Recovery:
 *   - Boot failures due to invalid BPB parameters can be checked by verifying filesystem geometry
 *     via debugging print routines (PrintLowNibble/PrintAL/PrintNumber) emulated in boot.asm.
 */

#include <stdint.h>

/* 
 * Memory layout for the LibreDOS FAT12/FAT16 boot process (copied from boot.asm):
 *
 *	...
 *	|-------| 1FE0h:7E00h = 27C00h (159 KiB)
 *	|BOOTSEC| loader relocates itself here first thing,
 *	|RELOC.	|  before loading root directory/FAT/kernel file
 *	|-------| 1FE0h:7C00h = 27A00h (158 KiB)
 *	|  gap  | PARAMS live here
 *	|LBA PKT| LBA disk packet
 *	|-------| 1FE0h:7BC0h = 279C0h (158 KiB)
 *	|  gap  | READADDR_* live here
 *	|-------| 1FE0h:7BA0h = 279A0h (158 KiB)
 *	| STACK | below relocated loader, above sector buffer (size 5.9 KiB)
 *	...
 *	|-------| 1FE0h:6400h = 26200h (152 KiB)
 *	|SEC.BUF| sector buffer, to avoid crossing 64 KiB DMA boundary (size 8 KiB)
 *	|-------| 1FE0h:4400h = 24200h (144 KiB)
 *	...
 *	|-------| 1FE0h:4380h = 24182h (144 KiB)
 *	|CLUSTER| built from FAT, listing every cluster of the kernel file.
 *	| LIST  |  file <= 134 KiB, cluster >= 32 Byte, hence <= 8578 B list.
 *	|-------| 1FE0h:2200h = 22000h (136 KiB)
 *	...
 *	|-------| 0000h:7E00h = 07E00h (31.5 KiB)
 *	|BOOTSEC| possibly overwritten by the FAT (<= 128 KiB) and the kernel,
 *	|ORIGIN |  so the bootsector relocates itself up...
 *	|-------| 0000h:7C00h = 07C00h (31 KiB)
 *	...
 *	|-------|
 *	|KERNEL	| maximum size 128 KiB (overwrites bootsec origin)
 *	|LOADED	| (holds directory then FAT before kernel file load)
 *	|-------| 0060h:0000h = 00600h (1.5 KiB)
 */

struct boot_sector_layout {
    uint8_t  jmp_boot[3];           // bp-Entry+0x00 Jump instruction to boot code
    uint8_t  oem_name[8];           // bp+0x03       OEM label
    uint16_t bytes_per_sector;      // bp+0x0b       bytes/sector
    uint8_t  sectors_per_cluster;   // bp+0x0d       sectors/allocation unit
    uint16_t reserved_sectors;      // bp+0x0e       # reserved sectors
    uint8_t  num_fats;              // bp+0x10       # of fats
    uint16_t max_root_entries;      // bp+0x11       # of root dir entries
    uint16_t total_sectors_short;   // bp+0x13       # sectors total in image
    uint8_t  media_descriptor;      // bp+0x15       media descrip: fd=2side9sec, etc...
    uint16_t sectors_per_fat;       // bp+0x16       # sectors in a fat
    uint16_t sectors_per_track;     // bp+0x18       # sectors/track
    uint16_t num_heads;             // bp+0x1a       # heads
    uint32_t hidden_sectors;        // bp+0x1c       # hidden sectors
    uint32_t total_sectors_long;    // bp+0x20       # sectors if > 65536
    // ... Extended parameters ...
    uint8_t  drive_number;          // bp+0x24       drive number
    uint8_t  ext_boot_signature;    // bp+0x26       extended boot signature
    uint32_t volume_id;             // bp+0x27       volume ID
    uint8_t  volume_label[11];      // bp+0x2b       volume label
    uint8_t  filesystem_type[8];    // bp+0x36       filesystem type ID ("FAT12" or "FAT16")
};
