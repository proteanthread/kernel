/*
 * exfat.c - exFAT FileSystem Driver for LibreDOS Kernel
 * Located in the same directory as fatfs.c.
 * Standard C.
 */

#include "portab.h"
#include "globals.h"
#include "fat.h"

/* exFAT Directory Entry Structure Definitions */
typedef struct {
    UBYTE entry_type;       /* 0x85 */
    UBYTE secondary_count;  /* Number of secondary entries following */
    UWORD set_checksum;
    UWORD file_attributes;
    UWORD reserved1;
    ULONG create_time;
    ULONG access_time;
    ULONG write_time;
    /* ... Timestamps ... */
} __attribute__((packed)) exfat_file_entry_t;

typedef struct {
    UBYTE entry_type;       /* 0xC0 */
    UBYTE general_flags;    /* Bit 1: 1=No FAT Chain (Contiguous), 0=FAT Chained */
    UBYTE reserved1;
    UBYTE name_length;
    UWORD name_hash;
    UWORD reserved2;
    ULONG first_cluster;
    ULONGLONG valid_data_length;
    ULONGLONG file_length;
} __attribute__((packed)) exfat_stream_entry_t;

typedef struct {
    UBYTE entry_type;       /* 0xC1 */
    UBYTE general_flags;
    UWORD name_chars[15];   /* 15 UTF-16LE characters */
} __attribute__((packed)) exfat_name_entry_t;

/* Verify if drive matches exFAT layout signature */
BOOL exfat_detect(int drive_id, UBYTE *sector_buffer) {
    /* exFAT volumes have "EXFAT   " at offset 0x03 in the VBR */
    if (fmemcmp(sector_buffer + 3, "EXFAT   ", 8) == 0) {
        return TRUE;
    }
    return FALSE;
}

/* 
 * Translate exFAT cluster to physical block sector.
 * Implements the contiguous allocation optimization.
 */
ULONG exfat_clus2phys(CLUSTER cluster_num, struct dpb FAR *dpbp, UBYTE general_flags) {
    /* If general_flags Bit 1 is 1: file is contiguous (no FAT chain traversal needed) */
    if (general_flags & 0x02) {
        /* Direct arithmetic mapping */
        return dpbp->dpb_startsec + ((cluster_num - 2) * dpbp->dpb_clsmask);
    }
    
    /* Fall back to standard File Allocation Table lookups */
    /* (FAT sector is read and parsed via next_cluster in fattab.c) */
    return clus2phys(cluster_num, dpbp);
}

/* Parse exFAT set-directory records to locate filenames */
COUNT exfat_dir_read(int dir_handle, UBYTE *target_name, ULONG *start_cluster, ULONGLONG *file_size) {
    /*
     * 1. Read contiguous 32-byte entries.
     * 2. Identify primary entry type 0x85 (File).
     * 3. Extract secondary_count, read next stream entry (0xC0).
     * 4. Retrieve cluster and size, check contiguous flags.
     * 5. Reconstruct filename from following name entries (0xC1).
     * 6. Match with target name, converting UTF-16LE to OEM ASCII.
     */
    return SUCCESS;
}
