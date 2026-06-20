# UEFI System Integration & Architecture Guide (`UEIF.md`)

This document defines the UEFI boot sequence, graphics console, and storage redirection layers for the **LibreDOS C Core** (`ld`). It provides guidelines on how the standard C17 implementation modernizes legacy BIOS interrupt routines to execute natively on modern UEFI-based motherboards.

---

## 1. Architectural Role of UEFI in LibreDOS

Legacy x86 DOS environments rely on real-mode x86 BIOS interrupts:
- `Int 10h` for character console and VGA framebuffers.
- `Int 13h` for disk operations (CHS/LBA sector reading and writing).
- `Int 14h` for serial communications.
- `Int 15h` (E820h) for system memory maps.

Modern UEFI motherboards operate entirely in 32-bit or 64-bit protected/long mode and **do not expose BIOS interrupt vectors**. To support UEFI boots natively without virtualization overhead:
1. **Bootloader Execution**: The UEFI firmware boots via standard partition files, executing a compiled `BOOTX64.EFI` binary from the EFI System Partition (ESP).
2. **Firmware Querying**: The bootloader queries the EFI System Table and UEFI Runtime Services to retrieve system hardware maps, Graphics Output Protocol (GOP) pointers, and input device descriptors.
3. **Core Initialization**: The bootloader maps a 1MB linear memory segment for guest execution (regulated by the **LibreDOS Memory Specification - LMS**), copies `KERNEL.SYS` to memory, and invokes the C kernel entry point with a pointer to the system boot parameters block.
4. **Device Abstraction**: The kernel maps console writes directly to the GOP framebuffer or serial UART ports, and sector reads/writes to UEFI block IO protocols.

---

## 2. UEFI Boot Sequence (`BOOTX64.EFI` -> `KERNEL.SYS`)

The boot progression is structured as follows:

```
+--------------------------------------------------------------+
|                   UEFI Firmware Boot Phase                   |
|  - Locates ESP partition                                    |
|  - Executes \EFI\BOOT\BOOTX64.EFI                            |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                     LibreDOS Bootloader                      |
|  - Allocates conventional 1MB segment (dos_chunk_t, Chunk 0) |
|  - Queries EFI System Table for GOP Framebuffer address       |
|  - Reads KERNEL.SYS from FAT/exFAT filesystem                |
+------------------------------+-------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                   C Kernel Entry (main.c)                    |
|  - Initializes LMS memory slab and page allocator            |
|  - Maps console writes directly to GOP Framebuffer           |
|  - Hooks serial console CON / UART to hardware registers      |
+--------------------------------------------------------------+
```

---

## 3. Graphics Output Protocol (GOP) & Console Redirection

On systems booting via UEFI, legacy VGA `Int 10h` screen writes are replaced by writing directly to the linear framebuffer:
1. The bootloader queries the `EFI_GRAPHICS_OUTPUT_PROTOCOL` (GOP) to locate the physical start address of the video framebuffer, pixel format, and resolution parameters.
2. The driver maps this physical framebuffer address into memory.
3. Console print commands in the C core (`console.c`) write character glyph bitmaps directly to pixel offsets within the framebuffer, bypassing legacy video segments (like `0xB8000`).

---

## 4. Mass Storage & Device Driver Redirects

For disk operations, LibreDOS maps disk reads/writes to UEFI Block IO protocols or direct hardware interfaces:
- **Raw Sector Translation**: The block driver translates logical DOS sectors directly into physical offsets on the underlying storage medium (e.g. SATA, NVMe, or SPI SD cards).
- **Virtual Redirection**: Int 21h file calls are intercepted by the C file system layer (`dosfns.c`, `fatfs.c`) and routed directly to host directories mapped on the UEFI partition.

---

## 5. Changeability & Constraints

### A. What Can Be Changed
- **Resolution Modes**: Screen mode configurations, font sizes, and GOP screen rendering techniques can be safely optimized or rewritten.
- **Buffer Configurations**: Sizes of bounce buffers and slab sizes of current directory caches.
- **Diagnostic Logging**: Verbose boot logging outputs and debug print interfaces.

### B. What Cannot Be Changed
- **Register Struct Layouts**: The layout of CPU registers inside context structs must not be reordered, as it directly mirrors assembly layouts in `entry.asm` and `procsupt.asm`.
- **Segment Address Mappings**: Memory-mapped offsets for legacy compatibility (such as the List of Lists base, Swappable Data Area, and the conventional memory 1MB boundary limit) must remain strictly compatible with MS-DOS specifications.

---

## 6. Expected Behavior

- **Protected Execution**: The kernel executes in flat 32-bit or 64-bit protected mode. Access to hardware devices is governed by physical memory page mapping tables rather than direct I/O port instructions.
- **Segment Translation**: All segment-offset pointers passed by guest applications are translated to flat physical addresses via `ld_translate_address` before reading or writing.

---

## 7. Diagnostics & Recovery (What to do if something breaks)

- **Black Screen / Boot Loop**:
  - Verify that the UEFI GOP parameters (framebuffer physical base, resolution, and scanline width) passed in the boot block match the values reported by the motherboard firmware.
  - Check that the font glyph data is compiled and linked correctly.
- **Memory Corruption / GPF**:
  - Check structural alignment rules (`__attribute__((packed))`) in C files like `kernel.c`. Misaligned structure fields will cause address translation failures when mapping parameters.
  - Review segment boundary limits in the map files (e.g. `kernel.map`) to ensure the stack does not overrun structural segments.
- **File System / Drive Mount Failures**:
  - Verify that the target ESP partition format (FAT12/16/32 or exFAT) matches the drive parameters in `boot.c` / `boot32.c`.
