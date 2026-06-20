# LibreDOS C Abstraction Architecture & Porting Guide (`ld`)

This document defines the architecture, design, and porting specifications for the **LibreDOS C Core** (`ld`). This subsystem ports legacy 16-bit assembly routines to standard C, introducing the **1MB Hybrid Memory Model** (governed by the overall **LibreDOS Memory Specification - LMS**) and hybrid drive redirection to support native boots on UEFI motherboards and IoT platforms (ESP32, Arduino, Raspberry Pi).

---

## 1. Assembly-to-C Porting Specification

Modern motherboards booting via UEFI and non-x86 microcontrollers (ARM, Xtensa, AVR) do not support x86 16-bit real-mode execution. To enable compatibility, all legacy assembly files are translated into platform-independent C, with the new `.c` files placed directly alongside their corresponding `.asm` files in the same directory.

### A. Porting `entry.asm` (Interrupt Entry & Stack Routing)
The legacy assembly interrupt dispatcher switches stacks between user stack, char stack, error stack, and disk stack to avoid stack overflows. In `entry.c`, this stack-shifting is replaced with structured thread-level stack contexts in C:

```c
typedef struct {
    uint32_t AX, BX, CX, DX;
    uint32_t SI, DI, BP, SP;
    uint16_t DS, ES, FS, GS, SS, CS;
    uint32_t Flags;
} regs_context_t;

/* C Interrupt API Router */
void ld_dispatch_interrupt(uint8_t int_no, regs_context_t *regs) {
    switch (int_no) {
        case 0x21:
            ld_int21_router(regs);
            break;
        case 0x25:
            ld_int25_handler(regs);
            break;
        case 0x26:
            ld_int26_handler(regs);
            break;
        default:
            ld_default_handler(int_no, regs);
            break;
    }
}
```

### B. Porting Context Switches (`procsupt.asm` to `procsupt.c`)
Instead of executing hardware context pushes (`pusha`, `popa`) to change tasks, C structures store, read, and switch the context pointers when executing `exec_user()` or `return_user()`.

### C. Porting String & Memory Routines (`asmsupt.asm` to `asmsupt.c`)
Standardized optimization is achieved by replacing low-level assembly string scan loops with standard C pointer sweeps or leveraging highly optimized library functions (`memcpy`, `memset`, `memcmp`, `strlen`, `strcmp`).

---

## 2. LibreDOS Memory Specification (LMS) & The 1MB Hybrid Memory Model

To maintain 100% PC-DOS and MS-DOS application compatibility in a flat 64-bit environment, the physical or host virtual memory is organized under the **LibreDOS Memory Specification (LMS)** using the **1MB Hybrid Memory Model** to partition RAM into 1MB segment chunks. 

At the same time, native application layers and the core kernel can access the entire available physical RAM (from 1GB up to 64GB or more) directly through flat 64-bit pointers. Legacy real-mode compatibility is achieved by encapsulating the traditional 16-bit real-mode address spaces inside logical segment mappings, while modern drivers and native applications execute without these constraints.

```
       +-------------------------------------------------------------+
       |                  64-bit Flat Memory Space                   |
       |                (Host RAM Pool: 1GB to 64GB+)                |
       +------------------------------+------------------------------+
                                      |
              +-----------------------+-----------------------+
              v (Dynamic Page Pager)                          v (Native Direct Mode)
       +-----------------------------+                  +----------------------------+
       |   Mapped into 1MB Chunks    |                  |  Full Flat Physical RAM    |
       |     for Legacy Guests       |                  |  (For 64-bit Kernel/Apps)  |
       +--------------+--------------+                  +----------------------------+
                      |
                      v Segment translation
       +---------------------------------------------+
       |  Conventional RAM (0 - 640KB)               |
       |  Upper Memory Blocks (640KB - 1MB)          |
       |  High Memory Area (HMA - above 1MB boundary)|
       +---------------------------------------------+
```

### A. Dynamic Physical Allocator (DLA)
The host C core implements a dynamic allocator inspired by the BSD virtual memory (VM) pager and slab system:
1. **Dynamic Page Pager**: Allocates and maps physical memory in standard 4KB pages.
2. **Slab Allocator**: Optimizes small kernel memory footprint for structures such as System File Tables (SFT), Current Directory Structures (CDS), and Memory Control Blocks (MCB).
3. **Chunk Pager**: Allocates contiguous 256-page pools (1MB) as logical sandbox contexts for guest runs.

### B. Segment Translation Layer
Legacy binaries read and write segment-offset references. The C memory translator maps these references into the active 1MB chunk base:

```c
#define CHUNK_SIZE_1MB 1048576
#define XMS_MAX_HANDLES 128
#define EMS_MAX_PAGES 2048

typedef struct {
    uint8_t *handle_address;
    uint16_t size_pages;
    uint16_t physical_page;
} ems_handle_t;

typedef struct {
    uint8_t *base_address;    /* Flat 64-bit physical address pointer */
    uint32_t chunk_id;        /* Unique virtual system ID */
    
    /* Memory specifications state */
    uint16_t umb_start_seg;
    uint16_t umb_size_paras;
    
    struct {
        uint8_t *ptr;
        uint32_t size_bytes;
        uint8_t  in_use;
    } xms_handles[XMS_MAX_HANDLES];
    
    struct {
        ems_handle_t handles[XMS_MAX_HANDLES];
        uint8_t *page_frame;
        uint8_t *page_pool;
    } ems;
} dos_chunk_t;

/* Translate 16-bit Segment:Offset pointer to 64-bit Flat Address */
inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk) {
    uint32_t linear_offset = ((uint32_t)segment << 4) + offset;
    return chunk->base_address + (linear_offset % CHUNK_SIZE_1MB);
}
```

This ensures that critical DOS structures like the **List of Lists (LoL)** and **Swappable Data Area (SDA)** reside at correct relative offsets inside Chunk 0.

### C. Classic Memory Specifications Mapping

#### 1. Upper Memory Blocks (UMB)
When configuration requests `DOS=UMB` or drivers call `DosUmbLink()`, the memory manager registers the unused region of the Upper Memory Area (UMA) between `A0000h` and `FFFFFh` as a UMB pool inside the current `dos_chunk_t`. The allocator creates a standard MCB at `uppermem_root` within that block, which links directly to the end of the conventional memory MCB list, preserving standard DOS FIRST_FIT/BEST_FIT/LAST_FIT UMB allocations.

#### 2. eXtended Memory Specification (XMS)
The XMS interface is virtualized by registering a handler at `Int 2Fh AX=4310h`:
- **Extended Memory Allocation**: Requests for XMS blocks (XMS function `09h`) are routed directly to the host's Dynamic Page Pager, which allocates memory pages from the full 64-bit host RAM pool.
- **Move Block Function**: Emulated via C-level `memcpy` rather than legacy BIOS real-to-protected mode transition wrappers, avoiding virtualization overhead.
- **Capacity**: Enables legacy programs to allocate up to 4GB of memory per handle (using XMS 3.0 API), while native modules access the full 64-bit physical host RAM.

#### 3. Expanded Memory Specification (EMS)
EMS (LIM EMS 4.0) is emulated via `Int 67h`:
- **Page Frame**: A 64KB block within the chunk's UMA (typically segment `D000h` or `E000h`) is registered as the EMS page frame.
- **Bank-Switching**: When the application requests mapping logical page `N` to physical page `P` (EMS function `44h`), the page frame mapping logic dynamically adjusts the internal pointer address for page `P` inside the `dos_chunk_t` state to target the host EMS page pool memory block.

---

### D. Virtualization & Extensibility: Consoles, Terminals, VMs, and Systems
By dividing host RAM into discrete `dos_chunk_t` descriptors, the LMS **1MB Hybrid Memory Mode** functions as a virtualization manager:
1. **Virtual Machines & Virtual Systems**: 
   - Multiple separate `dos_chunk_t` instances can run concurrently. A lightweight core context scheduler executes time-sliced switching between the different chunk registers, allowing multi-instance DOS systems to execute in parallel.
2. **Virtual Consoles & Virtual Terminals**:
   - The screen framebuffer (`0xB8000` text or `0xA0000` graphics) within a chunk is virtualized. 
   - An active virtual console maps its buffer directly to the physical display (via UEFI GOP or a serial console). Switching consoles redirects keyboard event queues to the target chunk's input queue and copies its video cache to the active output device.

---

## 3. Hybrid Filesystem: Virtual Redirection & Raw Disk

To run on real hardware while supporting user-space environments, the filesystem layer is built as a hybrid:

1. **Raw Sector-Based Mounts**: Maps hard drive partitions directly. Sector arrays are read or written through standard FAT12/16/32 and exFAT layout engines.
2. **Path Redirection (Virtual Drives)**: Intercepts directory paths during `Int 21h` calls and routes them to directories on the host operating system (such as mapping `C:\` to local files on UEFI flash drives or microcontroller partitions).

---

## 4. Booting UEFI & IoT Platforms

### A. UEFI Boot Sequence (No MBR Support)
Modern UEFI motherboard firmware boots using partition files (specifically executing `BOOTX64.EFI` from an ESP partition) rather than executing code in sector 0 (MBR).
1. The LibreDOS bootloader is compiled as a standard EFI executable.
2. The bootloader queries the EFI System Tables to acquire screen GOP pointers and physical memory descriptors.
3. It loads the C kernel (`KERNEL.SYS`) into RAM, sets up the initial `dos_chunk_t` parameters (Chunk 0 base), and calls the C main entry point, passing the system configuration parameters block.

### B. Microcontroller Boots (ESP32, Arduino, Raspberry Pi)
For IoT devices that lack traditional BIOS or operating system wrappers:
- The C kernel is compiled directly into the device's firmware binary.
- Character console routines in [chario.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/chario.c) are mapped directly to hardware UART pins.
- Disk accesses (`dsk.c`) translate sector requests directly into SD card reads or Flash offset writes.

---

## 5. Architectural Maintenance Guidelines

### A. What We Can Change
- **Allocator Logic**: You can customize or tune the dynamic physical page pager or slab allocators (e.g. changing slab size constants or optimization policies).
- **Consoles & Terminals Mapping**: The active video redirect routing (how the screen buffer is copied to UEFI GOP or hardware UART pins) can be modified.

### B. What We Cannot Change
- **1MB Boundary Translation**: The math used in `ld_translate_address` and the strict 1MB size limit (`CHUNK_SIZE_1MB`) are fixed; shifting these boundary layouts will corrupt memory segment accesses.
- **List of Lists Segment Layout**: The standard structures (LoL, Swappable Data Area (SDA), MCBs) must remain at their expected offset positions inside conventional segment limits.

### C. What to Expect
- **Thread Stacks Context**: Stacks are segmented and managed context-by-context; switching contexts switches both register descriptors and active stack frames.
- **Dynamic Physical Allocation Paging**: Memory is paged via standard 4KB blocks. Dynamic allocations outside the 1MB sandbox chunks are mapped directly using host pointers.

### D. What to Do If Something Breaks / Troubleshooting
- **Memory Leak Inspections**: If the system runs out of pages under intense load, trace allocate and free balances in `lms_allocate_pages` and `lms_free_pages`.
- **Context Isolation Audit**: Check that segment references inside a chunk never access memory outside its allocated base plus 1MB boundary limit.

