# LibreDOS C Abstraction Architecture & Porting Guide (`ld`)

This document defines the architecture, design, and porting specifications for the **LibreDOS C Core** (`ld`). This subsystem ports legacy 16-bit assembly routines to standard C, introducing a 1MB hybrid memory segmentation model and hybrid drive redirection to support native boots on UEFI motherboards and IoT platforms (ESP32, Arduino, Raspberry Pi).

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

## 2. The 1MB Hybrid Memory Model

To maintain 100% PC-DOS and MS-DOS application compatibility in a flat 64-bit environment, the physical or host virtual memory is organized into **1MB segment chunks**.

```
       +---------------------------------------------+
       |           64-bit Flat Memory Space          |
       |               (UEFI / Host RAM)             |
       +----------------------+----------------------+
                              |
                              v  Mapped into 1MB Chunks
       +---------------------------------------------+
       |   Chunk 0 (0 to 1MB)   |   Chunk 1 (1 to 2MB) |
       |   Conventional Memory  |   Extended Space     |
       +---------------------------------------------+
                              |
                              v  Segment translation
       +---------------------------------------------+
       |  Conventional RAM (0 - 640KB)               |
       |  Upper Memory Blocks (640KB - 1MB)          |
       |  High Memory Area (HMA - above 1MB boundary)|
       +---------------------------------------------+
```

### Segmentation Translation:
Legacy binaries read and write segment-offset references. The C memory translator maps these references into the active 1MB chunk base:

```c
typedef struct {
    uint8_t *base_address;    /* Flat 64-bit physical address pointer */
    uint32_t chunk_size;      /* Fixed at 1MB (1048576 bytes) */
} dos_chunk_t;

/* Translate 16-bit Segment:Offset pointer to 64-bit Flat Address */
inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk) {
    uint32_t linear_offset = ((uint32_t)segment << 4) + offset;
    return chunk->base_address + (linear_offset % CHUNK_SIZE_1MB);
}
```

This ensures that tables like the **List of Lists (LoL)** and **Swappable Data Area (SDA)** reside at correct relative boundaries inside Chunk 0, preventing legacy applications from encountering memory faults.

### Virtualization & System Extensibility:
By partitioning memory into logical 1MB virtual segment boundaries, this hybrid model serves as the baseline for future integrations of **virtual consoles, virtual machines, virtual terminals, and virtual systems**. It enables running isolated virtual guest instances directly inside independent 1MB segment spaces mapped by the central 64-bit kernel memory manager.

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
