# LibreDOS Kernel Architecture & Design Guide

This document details the architecture, execution model, and memory segment layout of the LibreDOS (`DOS-C`) kernel. It also provides an integration plan for modernization, specifically focusing on **exFAT** filesystem support.

---

## 1. Low-Level Startup Flow

The boot sequence of the LibreDOS kernel is a carefully orchestrated transition from real-mode 16-bit assembly bootstrap code to structured C execution.

```
+--------------------------------------------------------+
|                 BIOS / Boot Sector                     |
|  (boot.asm / boot32.asm loads KERNEL.SYS to 0060h:000) |
+---------------------------+----------------------------+
                            |
                            v
+--------------------------------------------------------+
|                 kernel.asm: entry                      |
|  - Jump to realentry, query CPU type, save boot unit  |
|  - Parse command line passed in stack from bootloader  |
+---------------------------+----------------------------+
                            |
                            v
+--------------------------------------------------------+
|                 kernel.asm: relocations                |
|  - Move INIT_TEXT & INIT_DATA to high conventional mem  |
|  - Move HMA_TEXT & HMA_DATA to HMA segment (if active) |
|  - Align stack and segment registers (SS, DS, ES, CS)  |
+---------------------------+----------------------------+
                            |
                            v
+--------------------------------------------------------+
|                 main.c: LibreDOSmain()                  |
|  - Call setup_int_vectors() to register DOS API hooks |
|  - Call init_kernel() to mount disks and parse config |
+---------------------------+----------------------------+
                            |
                            v
+--------------------------------------------------------+
|                 main.c: init_kernel()                  |
|  - Init OEM hooks, load standard character devices     |
|  - Parse CONFIG.SYS (DoConfig) and load device drivers  |
|  - Re-allocate caches and file tables (PostConfig)     |
+---------------------------+----------------------------+
                            |
                            v
+--------------------------------------------------------+
|                     main.c: kernel()                   |
|  - Construct command-line tail for COMMAND.COM         |
|  - Execute process 0 (the command shell) via EXEC API |
+--------------------------------------------------------+
```

### Detailed Initialization Stages:
1. **Bootloader Hand-off**: The boot sector (compiled from [boot.asm](file:///C:/Users/rtdos/GitHub/kernel/boot/boot.asm) or [boot32.asm](file:///C:/Users/rtdos/GitHub/kernel/boot/boot32.asm)) loads `KERNEL.SYS` into memory at segment `0060h`. The boot drive number (BIOS ID) is passed in register `DL` (or `BL` in newer versions).
2. **Assembly Entry**: Execution enters the first byte of `KERNEL.SYS` in [kernel.asm](file:///C:/Users/rtdos/GitHub/kernel/kernel/kernel.asm) (`entry` point). 
   - `realentry` saves the boot load unit (`bl`) and stack pointer.
   - It checks for the presence of a debugger and copies the configuration parameters block (`_LowKernelConfig`) to the resident data area.
   - It checks CPU limits (ensuring target processor matches compiler optimizations like 80186 or 80386).
3. **Memory Relocation**: The kernel moves itself to higher segments in memory to prevent being overwritten by user executables.
   - If the High Memory Area (HMA) is available via A20 line enable, `HMA_TEXT` and resident data are moved above the 1MB boundary (`FFFF:0000`).
   - Non-resident initial startup logic (`INIT_TEXT`, `INIT_DATA`) is kept in high conventional memory and freed once the shell starts.
4. **C Transition**: Segment registers are synchronized, and execution jumps via a far return (`retf`) to `_LibreDOSmain` in [main.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/main.c).
5. **Interrupt Vector Setup**: `setup_int_vectors()` hooks the primary processor interrupts:
   - `Int 21h`: Primary DOS API Router.
   - `Int 25h` / `Int 26h`: Absolute disk read/write.
   - `Int 20h` / `Int 27h`: Terminate / Terminate and Stay Resident (TSR).
   - `Int 24h`: Critical error handler.
   - `Int 2Fh`: Multiplex interrupt (drivers, SHARE, NLS).
6. **Config Parsing & Device Chaining**: `init_kernel()` runs:
   - Registers standard devices (`CON`, `AUX`, `PRN`, `CLOCK$`, `NUL`) into the linked list driver chain.
   - Invokes `DoConfig()` which parses `CONFIG.SYS` in three passes:
     - **Pass 0**: Parse basic buffers, blocks, and filesystem allocation options.
     - **Pass 1**: Load dynamic drivers via `DEVICE=` or `DEVICEHIGH=` statements.
     - **Pass 2**: Finalize memory planning and upper memory blocks (UMBs).
7. **Shell Execution**: The kernel prepares the environment blocks, replicates the system file tables (SFTs) to establish standard input/output/error channels, and triggers `init_call_p_0()` to execute `COMMAND.COM` via the C task interface.

---

## 2. Memory Segment Layout

The kernel divides code and data into functional segments to maximize the available conventional memory (the "640KB limit").

| Segment Name | Class | Memory Location | Description |
| :--- | :--- | :--- | :--- |
| **PSP** | `DATA` | Bottom of kernel memory | Serves as a scratch area for the initial process, simulating a Program Segment Prefix block. |
| **INIT_TEXT** | `CODE` | High Conventional | Initialization functions (e.g., `init_kernel`, `DoConfig`) that are discarded post-boot. |
| **INIT_DATA** | `DATA` | High Conventional | Temporary tables and text buffers utilized during system setup and startup messages. |
| **HMA_TEXT** | `CODE` | HMA (`FFFF:0010`+) | Main kernel execution loop, system call routers, and API implementation routines. |
| **HMA_DATA** | `DATA` | HMA or Conventional | Disk sector cache buffers, file node arrays, and memory tables. |
| **DGROUP** | `GROUP` | Conventional Data | The primary unified data group segment containing the kernel's resident global variables. |
| **_LOWTEXT** | `CODE` | Low Memory | BIOS interface vectors and low-level disk interrupt wrappers (such as `Int 13h` handler). |
| **_FIXED_DATA**| `DATA` | Low Memory | List of Lists anchor, Swappable Data Area (SDA), and local DOS buffers. |

---

## 3. Drive Letter Assignment (DLA)

DLA maps physical partitions onto drive letters (`A:` through `Z:`). The LibreDOS kernel handles this in [initdisk.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/initdisk.c):

1. **Floppy Disk Detection**: Query bios equipment lists to register floppy drives (assigned as `A:` and `B:`).
2. **Hard Disk Partitions**:
   - The kernel walks the Master Boot Record (MBR) and GUID Partition Tables (GPT) on all physical drives sequentially.
   - Partition assignment modes (configured via `PartitionMode` and `DLASortByDriveNo` in the kernel configuration area):
     - **Standard Mode (`DLASortByDriveNo = 0`)**: Walks all hard drives sequentially to assign primary partitions first (`C:`, `D:`, etc.), and then sweeps back to assign logical partitions located inside extended partition blocks.
     - **Drive-by-Drive Mode (`DLASortByDriveNo = 1`)**: Maps all partitions (primary and logical) on the first physical hard drive completely before moving to the next drive. This preserves drive letter stability when adding new physical drives.
3. **Current Directory Structure (CDS)**: For each assigned drive, the kernel constructs a `struct cds` (defined in [cds.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/cds.h)) tracking the current directory path, drive flag (physical, network, joined, or redirected), and a pointer to its Drive Parameter Block (DPB).

---

## 4. FileSystem & exFAT Integration Design

To support modern, high-capacity drives, the 16-bit LibreDOS kernel must be updated to support the **exFAT** filesystem. The existing filesystem layer is structured around the `FAT12`, `FAT16`, and `FAT32` structures.

### Current FAT Architecture:
- **`next_cluster()`** in [fattab.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/fattab.c): Walks the File Allocation Table sector-by-sector to find the next cluster index.
- **`dir_read()` / `dir_write()`** in [fatdir.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/fatdir.c): Traverses 32-byte directory entries, matching standard short names or reconstructing long filenames (LFNs) from consecutive LFN entries.
- **`clus2phys()`** in [fatfs.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/fatfs.c): Translates virtual file clusters into physical sector offsets using the Drive Parameter Block (`struct dpb`).

### exFAT Integration Strategy:

To integrate exFAT without breaking existing FAT support, we must implement an abstraction layer or modify the core functions to handle exFAT's unique layout:

```
                  +-----------------------------------+
                  |        Int 21h File Services      |
                  |     (dosfns.c / inthndlr.c)       |
                  +-----------------+-----------------+
                                    |
                                    v
                  +-----------------+-----------------+
                  |      Filesystem Router Layer      |
                  |  (Identifies format: FAT vs exFAT)|
                  +--------+-----------------+--------+
                           |                 |
                           v                 v
           +---------------+---+         +---+---------------+
           |    FAT12/16/32    |         |       exFAT       |
           | (fatfs.c, fattab.c)|         | (exfat.c - NEW)   |
           +-------------------+         +-------------------+
```

#### A. exFAT Drive Parameter Block (DPB) Extension
The current `struct dpb` (defined in [fat.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/fat.h)) uses 16-bit or 32-bit values for sector counts. For exFAT, it must be extended or wrapped to support:
- **64-bit sector boundaries**: Allocation units and file sizes up to 16 Exabytes.
- **Cluster allocation bitmap**: exFAT uses a continuous allocation bitmap to track free/used clusters, avoiding the need to traverse a traditional FAT table for contiguous files.
- **Large cluster sizes**: Supporting up to 32MB clusters (FAT32 is limited to 32KB or 64KB).

#### B. Directory Entry Processing
The directory structures must be updated:
- exFAT replaces traditional directory entries with **Set-Directories**:
  - **File Directory Entry (0x85)**: Declares file attributes, creation/modification times.
  - **Stream Extension Directory Entry (0xC0)**: Declares allocation flags (contiguous vs. fragmented), starting cluster index, and 64-bit valid data length/file length.
  - **File Name Directory Entry (0xC1)**: Contains up to 15 Unicode characters of the name. Multiple name entries are chained to represent names up to 255 characters.
- A new file reader/writer module, `exfat.c`, must be introduced to implement directory scans, parsing these variable-length entry sets and translating the UTF-16LE file names into the caller's target codepage.

#### C. API and Offset Modernization
- **64-bit File Offsets**: Traditional DOS APIs pass file offsets via `DX:AX` (32-bit offset). exFAT requires supporting 64-bit offsets. This means updating `DosRWSft()` and `DosSeek()` to process 64-bit positions when accessed by newer system call variants (or emulated internally using segmented offsets).
- **Contiguous File Optimization**: When the stream extension entry indicates a file is contiguous (no FAT chain), the read/write pipeline should calculate the physical sector offset directly via simple arithmetic (`start_sector + (offset / sector_size)`) rather than querying a cluster mapping chain, bypassing `next_cluster()` lookup overhead completely.

---

## 5. Kernel Maintenance and Architectural Guidelines

### A. What We Can Change
- **Allocator Slabs Size**: Constants defining memory allocation block sizes for SFTs, CDS, and MCB lists.
- **Buffer Pool Configuration**: The number and sizes of disk sector buffers managed inside [kernel\blockio.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/blockio.c).

### B. What We Cannot Change
- **List of Lists Anchors**: The relative segment position and access offsets of the primary List of Lists (LoL) anchor.
- **SDA Parameters Order**: The ordering and byte alignments of variables within the Swappable Data Area (SDA).

### C. What to Expect
- **Segment Isolation**: The kernel segregates code execution into `INIT_TEXT` (discarded) and `HMA_TEXT` (resident in HMA).
- **Guest VM Memory Page Translations**: Logical addresses mapped inside guest contexts are translated dynamically by the LMS segment routing layer.

### D. What to Do If Something Breaks / Troubleshooting
- **Structural Offsets Validation**: If kernel structs are corrupted at runtime, compile with assembly listing files enabled (`.lst`) and compare field offsets between C headers and assembly equates.
- **Boot Relocation Check**: Verify segment values inside `kernel.map` to ensure HMA relocations did not overlap reserved system segments.

