# LibreDOS Memory Specification (LMS) Tutorial & Reference Guide

This document defines the **LibreDOS Memory Specification (LMS)**, detailing the architecture, design, and implementation of the **1MB Hybrid Memory Mode** within the LibreDOS kernel core.

---

## 1. Introduction & Architectural Goals

Traditional real-mode x86 operating systems execute within a segmented 20-bit address space, limited to **1 Megabyte (MB)** of total physical RAM. Under modern 64-bit systems and microcontrollers, these segmentation boundaries do not exist natively.

To bridge this gap, the **LibreDOS Memory Specification (LMS)** establishes a virtualization and compatibility layer. The core objectives of LMS are:
1. **100% Application Compatibility**: Enforce standard real-mode segment-offset rules for legacy binaries.
2. **Flat memory access for native modules**: Enable modern drivers and native applications to access the full available host physical RAM (from 1GB to 64GB+) directly using flat 64-bit pointers.
3. **Multi-Instance Isolation**: Run multiple DOS systems or guest VMs in parallel, separating their memory spaces into isolated 1MB chunks.

---

## 2. The 1MB Hybrid Memory Mode

The **1MB Hybrid Memory Mode** divides the physical or host virtual memory into isolated 1MB partitions (called **Segment Chunks**).

```
   Host Flat Physical Memory Space (64-bit)
   +-------------------------------------------------------------+
   | Chunk 0 (System Core)                                       |
   | - Conventional memory (0 - 640KB)                           |
   | - Upper Memory Area (640KB - 1MB)                           |
   +-------------------------------------------------------------+
   | Chunk 1 (Guest VM / Alternate Console)                      |
   | - Isolated 1MB virtual real-mode container                 |
   +-------------------------------------------------------------+
   | Chunk N (Dynamic Heap / Native allocations)                 |
   | - Direct physical page allocations (above 1MB boundary)     |
   +-------------------------------------------------------------+
```

Each segment chunk is represented by a sandbox context descriptor (`dos_chunk_t` structure).
- **Chunk 0** serves as the primary system core, executing the resident kernel and primary shell.
- **Subsequent Chunks (1 to N)** are dynamically allocated to support virtual consoles, parallel DOS sessions, or guest applications.

---

## 3. Dynamic Physical Allocator (DLA)

The host memory manager implements a page-level allocator to feed the LMS memory requirements:
- **Dynamic Page Pager**: Manages physical host memory in **4KB page frames**.
- **Slab Allocator**: Optimizes small internal data structs (like SFTs, CDS nodes, and MCB blocks) within the kernel segment.
- **Chunk Allocator**: Requests contiguous blocks of 256 pages (1MB total) from the host RAM pool to construct a new `dos_chunk_t` context.

---

## 4. Segment Address Translation

When a program requests memory operations using 16-bit `Segment:Offset` logical pointers, the LMS translation layer maps the reference to a flat physical address within the target chunk:

```c
#define CHUNK_SIZE_1MB 1048576

inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk) {
    uint32_t linear_offset = ((uint32_t)segment << 4) + offset;
    return chunk->base_address + (linear_offset % CHUNK_SIZE_1MB);
}
```

This translation prevents logical segment wrapping (like crossing the 64KB barrier or addressing beyond 1MB) from corrupting other system memory spaces.

---

## 5. Memory Specifications mapping (UMB, XMS, EMS)

LMS emulates classic DOS memory extension standards by wrapping allocations through host services:

### A. Upper Memory Blocks (UMB)
Regions between `640KB` and `1MB` (segment `A000h` to `FFF0h`) are scanned. Free blocks are registered under the chunk's UMB pool. The memory allocator creates a standard Memory Control Block (MCB) in this space, linking UMBs directly to the end of conventional memory.

### B. eXtended Memory Specification (XMS)
XMS calls (via `Int 2Fh AX=4310h`) are virtualized:
- Requests to allocate Extended Memory Blocks (EMBs) bypass real-to-protected mode hardware wrappers.
- The manager queries the host Dynamic Page Pager directly to allocate pages from the full 64-bit host RAM pool.
- Block moves are executed using optimized C `memcpy` operations.

### C. Expanded Memory Specification (EMS)
EMS calls (via `Int 67h`) are emulated:
- A 64KB window in the chunk's UMA (typically segment `D000h`) acts as the EMS page frame.
- When an application maps pages (EMS function `44h`), the memory manager updates translation pointers within `dos_chunk_t` to redirect accesses inside the 64KB window to the host's EMS page pool.

### D. Compatibility with FreeDOS XMS/EMS Drivers (HIMEM, HIMEMX, EMM386)
To support existing memory managers used in FreeDOS when booting legacy applications:
- **INT 15h Memory Reporting**: The LMS layer emulates legacy BIOS memory querying interrupts (`INT 15h` sub-functions `AH=88h`, `AX=E801h`, and `AX=E820h`). It passes the correct physical memory map information to the guest environment, allowing FreeDOS `HIMEM.EXE`/`HIMEMX.EXE` to probe memory pools and control XMS registers.
- **Dynamic Intercept Override**: When the system detects that an external driver (like FreeDOS `EMM386.EXE` or `HIMEMX.EXE`) has registered itself via device headers or hooked interrupts (`INT 2Fh` or `INT 67h`), LMS dynamically disables its built-in XMS/EMS emulators, passing control directly to the loaded driver.
- **A20 Gate and HMA Emulation**: Standard A20 gate queries and segment wrap-around testing are virtualized. Real-mode instructions switching the A20 state (e.g. keyboard controller writes or INT 15h AX=2401h calls) are trapped and translated to enable accesses to the simulated High Memory Area (`FFFF:0010` to `FFFF:FFFF`).
- **Standard MCB Linkage**: Upper Memory Blocks (UMBs) created by external managers are linked into the DOS Memory Control Block (MCB) chain using standard FreeDOS rules, guaranteeing that memory allocation strategies (`LH`, `DEVICEHIGH`) operate identically.

---

## 6. Architectural Maintenance Guidelines

### A. What We Can Change
- **Allocation Policies**: You can change page allocation strategies (e.g. switching between First-Fit and Best-Fit page scans) or adjust memory heap boundaries.
- **Slab sizes**: The sizes of memory blocks pre-allocated for SFTs, current directories, and segment lists can be tuned to reduce footprint.
- **Console Page Mapping**: Redirection rules for mapping video buffers inside a chunk to different hardware targets (like GOP or UART).

### B. What We Cannot Change
- **1MB Boundary Constraints**: Segment address translation algorithms and the strict `CHUNK_SIZE_1MB` size limits cannot be modified; changing these boundaries breaks segment offsets logic inside legacy code.
- **Linked-List Offsets**: Pointers mapping standard DOS anchors (such as the List of Lists and SDA segment variables) must remain at their historical offsets.

### C. What to Expect
- **Linear Address Translation**: All segment-offset pointers are translated to flat addresses before hardware executions.
- **Context Isolation**: Memory limits are strictly bounded to the chunk size; accessing memory outside the base plus 1MB results in address wrapping inside the active chunk.

### D. What to Do If Something Breaks / Troubleshooting
- **Segment Bounds Violation**: If memory accesses corrupt other structures, verify `ld_translate_address` math and audit register states around segment shifts.
- **Memory Allocation Failure**: If dynamic page allocation fails, inspect page allocations tracking registers to ensure freed pages are correctly released to the host pager pool.
