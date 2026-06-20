# LibreDOS exFAT Integration Tutorial & Reference Guide

This document is a comprehensive guide to understanding, using, and implementing the **exFAT** (Extended File Allocation Table) filesystem inside the LibreDOS kernel workspace.

---

## 1. Why Choose exFAT?

Modernizing the kernel requires a filesystem that bridges legacy DOS applications with modern host environments. We choose exFAT for several reasons:

1. **Universal Portability**: exFAT is natively read and written by Windows 10/11, macOS, and Linux without third-party drivers.
2. **No 4GB File Limit**: FAT32 restricts file sizes to 4GB minus 1 byte. exFAT expands this limit to **16 Exabytes (EiB)**, enabling modern databases, system backups, and logs.
3. **High Cluster Limits**: Supports cluster sizes up to **32 Megabytes (MiB)**, minimizing fragmentation on high-capacity storage.
4. **Performance via Allocation Bitmaps**: Unlike FAT12/16/32, which require traversing a chained File Allocation Table sector-by-sector, exFAT uses a continuous allocation bitmap to track free space. Contiguous files bypass the FAT chain lookup entirely, resulting in near-raw block speed.

---

## 2. Filesystem Comparison Table

| Feature | FAT12 | FAT16 | FAT32 | exFAT |
| :--- | :--- | :--- | :--- | :--- |
| **Bit Width** | 12-bit | 16-bit | 28-bit (active) | 32-bit |
| **Max Disk Size** | 32 MiB | 2 GiB (typical) | 2 TiB | 128 PiB (recommended limit) |
| **Max File Size** | 32 MiB | 2 GiB | 4 GiB - 1 byte | 16 EiB |
| **Max Cluster Size**| 8 KiB | 64 KiB | 32 KiB | 32 MiB |
| **Allocation Track**| FAT Chaining | FAT Chaining | FAT Chaining | FAT Chain + Allocation Bitmap |
| **Filename Limit** | 8.3 OEM ASCII | 8.3 OEM ASCII | 8.3 + LFN (overlapping) | Native 255 UTF-16LE characters |
| **Short-name (8.3)**| Required | Required | Required (for LFN link) | Optional (Not required) |
| **Time Precision** | 2 seconds | 2 seconds | 2 seconds | 10 milliseconds |

---

## 3. exFAT Directory Entry Specifications

In FAT32, directory entries are either static 32-byte blocks (for 8.3 short names) or a chain of virtual LFN entries preceding the primary entry. exFAT replaces this layout with **Set-Directories**, where a file or subdirectory is defined by a grouping of contiguous 32-byte entries:

```
+--------------------------------------------------------------+
| 1. File Directory Entry (0x85)                              |
|    - File attributes, timestamps (created, modified, access)  |
+--------------------------------------------------------------+
| 2. Stream Extension Entry (0xC0)                             |
|    - General flags (contiguous flag, allocation details)     |
|    - Starting cluster, 64-bit Valid Data Length & File Length|
+--------------------------------------------------------------+
| 3. File Name Entries (0xC1)                                  |
|    - Unicode characters of the name (up to 15 chars per entry)|
+--------------------------------------------------------------+
```

### Entry Types:
1. **File Directory Entry (`0x85`)**: Contains general file attributes (Read-only, Hidden, System, Directory, Archive) and sub-second creation/modification timestamps.
2. **Stream Extension Entry (`0xC0`)**:
   - Contains the **General Flags** byte:
     - `Bit 0` (Allocation Possible): Indicates if clusters are assigned.
     - `Bit 1` (No Fat Chain): **CRITICAL FLAG**. If set to `1`, the file is stored contiguously on disk. The kernel bypasses FAT reads and calculates blocks directly via math offsets.
   - Contains the **First Cluster** starting address (32-bit).
   - Contains the **64-bit File Length** and **64-bit Valid Data Length**.
3. **File Name Entry (`0xC1`)**: Contains name length and up to 15 UTF-16LE characters. If a name is 30 characters, two `0xC1` entries follow the stream entry.

---

## 4. User's Guide: Partitioning & Formatting

To prepare storage media using exFAT for LibreDOS:

### Formatting in Modern Windows:
1. Insert the drive (USB/SD Card/Hard Disk).
2. Right-click and choose **Format...**
3. Select **exFAT** under the File System dropdown.
4. Set the **Allocation unit size** (Default or up to 32MB). Click Start.

### Mounting under LibreDOS:
- Specify partition parameter modes inside `CONFIG.SYS` (e.g. mapping partition type `0x07` or GPT basic partitions).
- Use `SYS CONFIG KERNEL.SYS FORCELBA=1` to ensure LBA (Logical Block Addressing) extensions are active, as exFAT volumes generally exceed the CHS (Cylinder-Head-Sector) boundaries.

---

## 5. Programmer's Guide: Implementing exFAT

To integrate exFAT support into the LibreDOS kernel codebase:

### A. Core Header Definitions
Add exFAT structure layouts to [hdr\fat.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/fat.h) or create a new header `hdr\exfat.h`:

```c
struct exfat_file_entry {
    UBYTE EntryType;        /* 0x85 */
    UBYTE SecondaryCount;   /* Number of extension entries following */
    UWORD SetChecksum;
    UWORD FileAttributes;
    UWORD Reserved1;
    ULONG CreateTime;
    ULONG AccessTime;
    /* ... Timestamps ... */
};

struct exfat_stream_entry {
    UBYTE EntryType;        /* 0xC0 */
    UBYTE GeneralFlags;     /* Bit 1: 1=No FAT Chain, 0=Use FAT Chain */
    UBYTE Reserved1;
    UBYTE NameLength;
    UWORD NameHash;
    UWORD Reserved2;
    ULONG FirstCluster;
    ULONGLONG ValidDataLength;
    ULONGLONG FileLength;
};
```

### B. Routing File Read / Write Pipelines
Modify the block transfer functions inside [kernel\fatfs.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/fatfs.c):

1. **Format Detection**:
   - In `media_check()` or `dsk_init()`, check the boot sector signature. exFAT boot sectors feature the string `"EXFAT   "` at offset `0x03` (replacing older signatures like `"MSDOS5.0"` or `"FAT32   "`).
2. **Implementing `exfat_clus2phys()`**:
   - For standard FAT files, cluster chains are walked. For exFAT files:
     - Check the `No FAT Chain` bit in the file's stream entry.
     - If `1` (Contiguous file): Calculate the physical block sector directly:
       $$\text{Sector} = \text{DataStartSector} + (\text{Cluster} - 2) \times \text{SectorsPerCluster}$$
     - If `0` (Fragmented file): Fall back to scanning the File Allocation Table.
3. **Directory Scan Upgrades**:
   - Modify `dir_read()` in [kernel\fatdir.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/fatdir.c) to handle exFAT Set-Directories. Instead of scanning single 32-byte slots, read directory records in chunks: read type `0x85`, extract extension entries count, read stream entry `0xC0`, and collect the UTF-16LE filename strings from the following `0xC1` entries.
   - Implement a UTF-16LE to OEM ASCII conversion routine to match incoming search paths.

---

## 6. Allowed Changes & Modernization Adjustments

When implementing this tutorial into code, you are allowed to make the following design adjustments to fit a 16-bit real mode memory constraints:

- **Cluster Heap Cache**: exFAT allocation bitmaps can grow large (e.g. 128KB for a large drive). Since the real-mode kernel operates with small buffer segments, do not load the entire bitmap in memory. Instead, implement a **Sliding Window Cache** in [kernel\blockio.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/blockio.c) that loads only the active sector of the allocation bitmap.
- **Short Name Emulation**: Legacy applications require an 8.3 filename structure. When listing directories, if a file has a long exFAT Unicode name, automatically generate a virtual short name alias (e.g., `MYTEXT~1.TXT`) to pass back to `FindFirst`/`FindNext` API calls, maintaining compatibility with older programs.
- **Customizing Maximum Drive Size Limits**: While the exFAT specification physically supports drive sizes up to **128 PiB**, the allocation bitmap required for such sizes (up to 512 MiB of RAM) is far too large for a 16-bit real-mode address space or low-footprint embedded devices. Under LibreDOS, we are allowed to impose artificial limits on the maximum drive size (e.g., capping support at **2 TiB** or **8 TiB**) or enforce a minimum cluster size configuration during mount verification. This significantly decreases cache memory boundaries and ensures system stability.

---

## 7. exFAT Implementation Guidelines

### A. What We Can Change
- **exFAT Cache Sizes**: Sliding window cache sizes and maximum buffer limits loaded inside [kernel\blockio.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/blockio.c).
- **Directory Traverse Speedups**: Internal search indexes or caching structures used to accelerate matching of names in `dir_read`.

### B. What We Cannot Change
- **Disk Sector Sizing**: Standard sector configuration constraints (e.g. 512 bytes or 4096 bytes) and boot sector offsets.
- **Directory Entries Layout**: The structure and byte offsets of the standard 32-byte exFAT directory entry types (`0x85`, `0xC0`, `0xC1`).

### C. What to Expect
- **Cluster Allocation Tables**: Non-contiguous files will traverse the FAT sector-by-sector; contiguous files will bypass the FAT lookup completely using direct offset calculations.
- **Streams Checks**: The stream entry flags must be analyzed for every file open or search request to determine if FAT chaining is required.

### D. What to Do If Something Breaks / Troubleshooting
- **Disk Corruption Recovery**: If file contents return garbage, check if the contiguous flag (`No FAT Chain` bit) in the Stream Extension is set correctly.
- **Directory Traversal Hangs**: Inspect UTF-16LE conversion logic for infinite loops when scanning long names that extend across sector boundaries.

