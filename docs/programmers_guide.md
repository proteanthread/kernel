# LibreDOS Kernel Programmer's Guide & Modernization Manual

This document is a technical reference guide for programming the LibreDOS (`DOS-C`) kernel. It covers internal interfaces, core data structures, driver hooking mechanism, and the roadmap for modernizing the kernel into a 64-bit system and a portable embedded micro-kernel without losing MS-DOS/PC-DOS compatibility.

---

## 1. DOS API & Services Mapping

The kernel exposes standard DOS systems APIs via software interrupts:

### A. Int 21h: Primary DOS API Router
The main entry point for filesystem, memory, and process control. Primary functions include:
- **Character I/O**: `AH=01h` (Input with echo), `AH=02h` (Output char), `AH=09h` (Print string), `AH=0Ah` (Buffered input).
- **File Access**: `AH=3Ch` (Create), `AH=3Dh` (Open), `AH=3Eh` (Close), `AH=3Fh`/`AH=40h` (Read/Write), `AH=42h` (Lseek).
- **Directory Operations**: `AH=39h` (Mkdir), `AH=3Ah` (Rmdir), `AH=3Bh` (Chdir), `AH=47h` (Getcwd).
- **Process Management**: `AH=4Bh` (Exec/Load), `AH=4Ch` (Exit with status code).
- **Memory Management**: `AH=48h` (Allocate), `AH=49h` (Free), `AH=4Ah` (Modify block size).
- **Disk Management**: `AH=36h` (Get free space), `AH=19h`/`AH=0Eh` (Get/Set current drive).

### B. Int 2Fh: Multiplex Interrupt
Provides inter-process communication and dynamic driver hookups:
- `AX=1200h` to `AX=12FFh`: Internal DOS functions (e.g. SFT manipulation, directory searches).
- `AX=1600h` / `AX=160Ah`: Windows 3.x/9x enhanced mode integration check.
- `AX=1400h` to `AX=14FFh`: National Language Support (NLSFUNC) helper functions.
- `AX=1000h` / `AX=1001h`: SHARE.EXE lock tracking check.

### C. Low-Level Disk Services
- **Int 25h / Int 26h**: Absolute disk sector read/write (retains flag image on stack upon return for MS-DOS compat).
- **Int 24h**: Critical Error Handler. Executed when hardware or disk errors occur. Users write custom handlers returning `0` (Ignore), `1` (Retry), `2` (Abort), or `3` (Fail) in `AL`.

---

## 2. Internal Data Structures

To preserve 100% MS-DOS/PC-DOS compatibility, the layout of the following tables is hardcoded in memory, precisely matching DOS specification.

### A. List of Lists (LoL)
Located at `DATASTART` in [kernel.asm](file:///C:/Users/rtdos/GitHub/kernel/kernel/kernel.asm) and exposed via `Int 21h AH=52h`. It contains the anchor pointers of the operating system:

```
Offset  Size    Description
--------------------------------------------------
-0002h  WORD    Segment of the first Memory Control Block (MCB)
 0000h  DWORD   Pointer to the first Drive Parameter Block (DPB)
 0004h  DWORD   Pointer to the head of the System File Table (SFT)
 0008h  DWORD   Pointer to the CLOCK$ device driver header
 000Ch  DWORD   Pointer to the CON (console) device driver header
 0010h  WORD    Maximum bytes per sector (typically 512)
 0012h  DWORD   Pointer to disk buffer info structure
 0016h  DWORD   Pointer to the Current Directory Structure (CDS) array
 001Ah  DWORD   Pointer to FCB tables
 0020h  BYTE    Number of block devices (drives) configured
 0021h  BYTE    LASTDRIVE configuration value
 0022h  DHDR    Device chain root (points to NUL device driver)
```

### B. Swappable Data Area (SDA)
Retrieved via `Int 21h AH=5Dh AL=06h`. Points to `_internal_data` in [kernel.asm](file:///C:/Users/rtdos/GitHub/kernel/kernel/kernel.asm). It contains the entire execution context of the DOS filesystem layer:

```
Offset  Size    Description
--------------------------------------------------
 00h    BYTE    Critical Error Flag (ErrorMode)
 01h    BYTE    InDOS Flag (counts active DOS re-entries)
 02h    BYTE    Drive ID triggering the critical error
 04h    WORD    DOS format error code
 08h    DWORD   Pointer to device header failing the IO
 0Ch    DWORD   Current Disk Transfer Area (DTA) address
 10h    WORD    Current Process PSP segment (cu_psp)
 16h    BYTE    Current default drive number
```

### C. System File Table (SFT)
Contains open file states, sector positions, and lock flags. Struct defined in [sft.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/sft.h):
- Tracks reference counts (how many handles reference this entry).
- Tracks execution offset positions and access attributes (Read, Write, Share modes).

### D. Drive Parameter Block (DPB)
Stores disk geometries, starting sectors, sector-to-cluster mapping, and media byte status. Defined in [fat.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/fat.h).

### E. Memory Control Block (MCB)
Encloses every allocated block in conventional memory. Struct defined in [mcb.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/mcb.h):
- **Signature**: `0x4D` ('M' for middle block) or `0x5A` ('Z' for last block).
- **Owner PSP**: Segment address of process owning the block. `0x0000` indicates a free block, `0x0008` indicates system space.
- **Block Size**: Size of the allocated region in 16-byte paragraphs.

### F. Program Segment Prefix (PSP)
A 256-byte header prepended to every launched application (defined in [pcb.h](file:///C:/Users/rtdos/GitHub/kernel/hdr/pcb.h)). Features standard exits (`int 20h`), far calls, segment size, parent PSP linkage, environment segment pointer, default FCBs, and the command tail parameters at offset `80h`.

---

## 3. OEM Hooking & Device Drivers

Device drivers are structured as binary links in a chain starting at `LoL->nul_dev`:

1. **Strategy Entry Point**: The driver receives a pointer to a Request Packet (describing commands like READ, WRITE, INIT) and stores it in `_ReqPktPtr`.
2. **Interrupt Entry Point**: The kernel calls the interrupt handler to execute the request.
3. **Execution Routing**: [execrh.asm](file:///C:/Users/rtdos/GitHub/kernel/kernel/execrh.asm) wraps strategy and interrupt calls, ensuring segment registers match requirements.

---

## 4. Modernization & Micro-Kernel Roadmap

To transition this traditional codebase into a modernized 64-bit kernel and a micro-kernel for embedded devices (such as Arduino, ESP32, and Raspberry Pi), the following paths must be defined.

```
       +---------------------------------------------+
       |           Existing DOS-C Kernel             |
       |  (16-bit Real Mode, Segments, Bios-centric) |
       +----------------------+----------------------+
                              |
                              |  Modernization & Refactoring
                              v
       +----------------------+----------------------+
       |          Unified Portability Core           |
       |       (64-bit UEFI, exFAT support)          |
       +----------------------+----------------------+
                              |
                              |  Micro-kernel Split
                              v
       +----------------------+----------------------+
       |          Embedded Micro-Kernel              |
       |   - Flat Dynamic Memory (malloc/free)       |
       |   - Embedded Bus Drivers (SPI, I2C, GPIO)   |
       |   - Legacy MS-DOS API Emulator Layer        |
       +---------------------------------------------+
```

### A. Minimized Footprint Core (Micro-kernel)
For embedded systems and IoT, memory footprint must be minimal (e.g. less than 64KB of RAM for microcontroller deployment):
- **Conditional Compilation**: Introduce macros (`MICRO_KERNEL_CORE`) to strip out legacy 16-bit disk and character abstractions (e.g. removing old FCB logic in `fcbfns.c`, NLS tables in `nls_hc.asm`, and obsolete multiplexers).
- **Task abstraction**: Replace DOS task execution (`task.c`) with a lightweight task scheduler utilizing threads or cooperative fibers instead of real-mode PSP switches.

### B. Flat Dynamic Memory Allocation
Embedded systems run in flat memory spaces rather than segmented architectures:
- **MCB Replacement**: Replace MCB paragraphs traversal (`memmgr.c`) with a flat-heap `malloc` / `free` memory manager.
- **Eliminating Real-Mode Limits**: Remove HMA relocation (`inithma.c`), UMB links (`DosUmbLink`), and EMS/XMS routines. Memory references are translated to linear 32-bit or 64-bit pointers.

### C. Native Built-in Driver Interfaces
Rather than relying on x86 BIOS interrupts (like `Int 10h`/`Int 13h`/`Int 14h`/`Int 17h`), the micro-kernel must integrate native controllers:
- **GPIO, SPI, I2C, UART**: Introduce direct hardware register access or lightweight abstraction wrappers inside [drivers](file:///C:/Users/rtdos/GitHub/kernel/drivers) (e.g., adding `gpio.c`, `spi.c`, `i2c.c`).
- **Disk Interface**: Replace BIOS Int 13h in [dsk.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/dsk.c) with direct SD card controllers (SPI mode) or eMMC flash drivers.

### D. Preserving MS-DOS & PC-DOS Compatibility
Modernizing the system must not compromise compatibility with legacy applications. To achieve both flat execution and legacy support, the following emulation strategy is defined:

1. **V86 / Real-Mode Emulation Layer**:
   - On 64-bit platforms (where 16-bit real-mode is natively blocked in long mode), the kernel executes legacy 16-bit binaries inside an isolated **Virtual 8086 Emulation Container** (using software-based virtualization or x86 hardware virtualization features).
2. **Structure Mapping (API Translation)**:
   - When an application executes `Int 21h`, the virtualization handler intercepts the interrupt and maps the parameters:
     - 16-bit segment-offset pointers (e.g., segment `DS` offset `DX` pointing to a file name) are translated into flat 64-bit linear addresses.
     - The modern filesystem (supporting exFAT and long paths) performs the operation.
     - The output parameters are written back into the emulator registers.
3. **Compatibility Data Wrappers**:
   - The kernel maintains virtual representations of the **List of Lists (LoL)**, **System File Tables (SFT)**, and **Memory Control Blocks (MCB)** within the lower 1MB execution block of the emulator, allowing legacy TSRs and utilities that directly scan DOS memory to read correct segment offsets without thrashed signatures.
4. **Mapped 1MB Hybrid Memory Mode**:
   - Mappings divide the 64-bit flat physical workspace into virtual 1MB segment boundaries. This hybrid layout provides the architectural base for future implementations of **virtual consoles, virtual machines, virtual terminals, and virtual systems** by isolating multiple concurrent DOS program instances inside independent 1MB sandboxes.
