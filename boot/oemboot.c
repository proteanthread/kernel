/*
 * oemboot.c - Custom OEM Boot loader routines (C17 standard)
 *
 * Architectural Role:
 *   Specifies structural details of the OEM boot sector compatible with IBM's PC-DOS and
 *   Microsoft's MS-DOS. It matches the BPB and loader parameters mapped in oemboot.asm.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Field descriptions and secondary C helpers.
 *   - CANNOT BE CHANGED: Memory layout offsets. Must respect PC-DOS requirements where
 *     IBMBIO.COM and IBMDOS.COM must be loaded into segment 0x0070 (LOADSEG) and limited to 29KB.
 *
 * Expected Behavior:
 *   - Relocates the boot sector and loads up to 58 sectors (29KB) of the kernel (IBMBIO.COM)
 *     to 0x70:0 before passing control. Expects the BPB to remain at 0x0:7C00 and the directory
 *     entry to be at 0x50:0.
 *
 * Diagnostics & Recovery:
 *   - If the system prints "not a bootable disk" or similar, verify directory structures and ensure
 *     the kernel files reside at consecutive sectors on the disk.
 */

#include <stdint.h>

/* 
 * Memory layout for the LibreDOS OEM boot process (copied from oemboot.asm):
 *
 *	+--------+
 *	| CLUSTER|
 *	|  LIST  |
 *	|--------| 0000:7F00
 *	|LBA PKT |
 *	|--------| 0000:7E00  (0:BP+200)
 *	|BOOT SEC| contains BPB
 *	|ORIGIN  | 
 *	|--------| 0000:7C00  (0:BP)
 *	|VARS    | only known is 1st data sector (start of cluster 2)
 *	|--------| 0000:7BFC  (DS:[BP-4])
 *	|STACK   | minimal 256 bytes (1/2 sector)
 *	|- - - - |
 *	|KERNEL  | kernel loaded here (max 58 sectors, 29KB)
 *	|LOADED  | also used as FAT buffer
 *	|--------| 0070:0000 (0:0700)
 *	|DOS DA/ | DOS Data Area,
 *	|ROOT DIR| during boot contains directory entries
 *	|--------| 0000:0500
 *	|BDA     | BIOS Data Area
 *	+--------+ 0000:0400
 *	|IVT     | Interrupt Vector Table
 *	+--------+ 0000:0000
 */

struct oem_boot_sector_layout {
    uint8_t  jmp_boot[3];           /* bp-Entry+0x00 Jump instruction to boot code */
    uint8_t  oem_name[8];           /* bp+0x03       OEM label */
    uint16_t bytes_per_sector;      /* bp+0x0b       bytes/sector */
    uint8_t  sectors_per_cluster;   /* bp+0x0d       sectors/allocation unit */
    uint16_t reserved_sectors;      /* bp+0x0e       # reserved sectors */
    uint8_t  num_fats;              /* bp+0x10       # of fats */
    uint16_t max_root_entries;      /* bp+0x11       # of root dir entries */
    uint16_t total_sectors_short;   /* bp+0x13       # sectors total in image */
    uint8_t  media_descriptor;      /* bp+0x15       media descrip: fd=2side9sec, etc... */
    uint16_t sectors_per_fat;       /* bp+0x16       # sectors in a fat */
    uint16_t sectors_per_track;     /* bp+0x18       # sectors/track */
    uint16_t num_heads;             /* bp+0x1a       # heads */
    uint32_t hidden_sectors;        /* bp+0x1c       # hidden sectors */
    uint32_t total_sectors_long;    /* bp+0x20       # sectors if > 65536 */
    uint8_t  drive_number;          /* bp+0x24       drive number */
    uint8_t  reserved_ext;          /* bp+0x25       reserved */
    uint8_t  ext_boot_signature;    /* bp+0x26       extended boot signature */
    uint32_t volume_id;             /* bp+0x27       volume ID */
    uint8_t  volume_label[11];      /* bp+0x2b       volume label */
    uint8_t  filesystem_type[8];    /* bp+0x36       filesystem type ID ("FAT12" or "FAT16") */
};

void oemboot_main(void) {
    /* Executes custom OEM boot sequence (loads up to 29KB of kernel to 0x70:0) */
}
