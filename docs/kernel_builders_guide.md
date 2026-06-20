# LibreDOS Kernel Builder's & Customization Guide

This guide describes how to configure, customize, and build a custom LibreDOS kernel. It details how to enable or disable compile-time features, integrate new functions, and update code while maintaining compatibility with legacy MS-DOS and PC-DOS software.

---

## 1. Customizing Features via Compile-Time Switches

The kernel utilizes preprocessor macros to configure features. These can be defined in `build.bat` (via `/D [MACRO]`) or configuration makefiles under `.\mkfiles`.

### Core Feature Switches:
- **`WITHFAT32`**: Enables support for the FAT32 filesystem layout.
  - *Footprint impact*: Disabling this macro strips approximately 6KB to 8KB of disk logic, which is useful when building minimized boot sectors or targeting memory-constrained embedded cores.
- **`WITHLFNAPI`**: Enables Long File Name (LFN) APIs.
  - *Footprint impact*: Disabling this removes LFN translation handlers, restoring the kernel to standard 8.3 filename processing.
- **`WIN31SUPPORT`**: Enables hooks and broadcasts required to run Windows 3.1 in Enhanced Mode (e.g., Critical Section broadcasts, instance data structures tracking, and virtual device parameters).
- **`DEBUG`**: Enables internal print logs (`printf`) and diagnostic outputs.
  - *Warning*: Do not enable this in production releases, as it increases code size and slows down disk I/O due to console output overhead.
- **`DEBUG_PRINT_COMPORT`**: Redirects kernel debug printf output to a serial COM port instead of the console monitor (useful for debugging early boot stages).
- **`CUSTOM_BRANDING`**: Allows overriding the default startup sign-on string with custom developer text.

### Processor Optimization Switches:
- **`I86`**: Standard x86 instruction set compatibility.
- **`I186`**: Uses 80186-specific instructions (e.g. `push immediate`, shifts with immediate).
- **`I386`**: Compiles optimizations for 80386+ processors (enables 32-bit registers preservation and 32-bit address spaces in drivers).

---

## 2. Modifying & Adding Kernel Features

To expand kernel services or adapt the system for embedded micro-kernel deployment:

### A. Registering Custom System Calls
To add custom software APIs, integrate your handlers inside the primary dispatch loop:
1. Open [kernel\inthndlr.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/inthndlr.c).
2. Inside `int21_service()`, locate the switch statement on register `lr.AH`.
3. Add a new `case` handler.
4. Ensure returned status flags and data registers are copied back to the register structure parameter `r`.

### B. Integrating Built-in Devices
To add native hardware drivers (e.g. built-in SPI, I2C, or UART adapters for embedded systems):
1. Write the strategy and interrupt procedures matching the DOS device driver header layout (`struct dhdr`).
2. Add your driver link inside the initialization list in [kernel\main.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/main.c) under `InitIO()`.
3. The driver will automatically join the device driver chain and become queryable via standard handle openings.

---

## 3. Maintaining MS-DOS / PC-DOS Compatibility

> [!WARNING]
> Modifying kernel structures or stripping assembly services can break compatibility with legacy software. To prevent thrashed execution and application failures, adhere to the guidelines below.

### A. Do NOT Modify Documented Table Offsets
Legacy DOS applications, utilities, and TSRs (such as memory monitors, disk editors, and file managers) read internal DOS tables directly instead of calling system interrupts. You must preserve the layouts of:
- **List of Lists (LoL)**: Do not add, remove, or rearrange variables at offsets below `0022h` (the NUL device driver offset). The position of pointers (such as MCB head, CDS, SFT, and DPB) is fixed.
- **Swappable Data Area (SDA)**: The offsets of flags (such as `ErrorMode` and `InDOS`) and critical directories parameters must match standard locations to allow TSRs to monitor filesystem states.
- **Memory Control Block (MCB)**: Do not modify the 5-byte header structure (Signature, Owner PSP, Size).

### B. Preserving Interrupt Registers Contexts
- When writing custom interrupt interceptors or C-level routers, ensure register states are fully preserved.
- When an application executes an interrupt:
  - Push all registers (`PUSH$ALL` in assembly or `Protect386Registers`) before dispatching to C.
  - Standard flags (like the **Carry Flag** to indicate errors) must be written back to the exact flags offset of the saved register image.

### C. Compatibility Mapping for exFAT / 64-bit Transitions
When modernizing the filesystem to exFAT or upgrading to 64-bit execution:
- Legacy applications expect a 32-bit sector representation and 32-bit file seek bounds.
- **Translation Layer**: If a 16-bit program calls a read/write or seek command, intercept the call and map the data through a translation wrapper:
  - Truncate offsets to 32-bit or return an out-of-range error if access exceeds 2GB/4GB limits.
  - Do not expose extended 64-bit layouts directly to legacy code.

---

## 4. Compilation & Build Orchestration

To compile and verify modifications:

1. **Configure Environment**: Set up paths inside `config.bat` (copied from `config.b`).
2. **Execute Build**:
   ```cmd
   build.bat [compiler] [cpu] [fat-type] [upx]
   ```
   - *Example for Watcom, 386 target, and FAT32*:
     ```cmd
     build.bat wc 386 fat32 upx
     ```
3. **Clobber/Clean**:
   - To clean intermediary files: `clean.bat`.
   - To force a full rebuild: `clobber.bat`.

---

## 5. Compiling for C 64-bit Hybrid Core

For booting on modern UEFI motherboards and IoT processors:
- The newly converted C files are written in **standard C** and are placed alongside the corresponding assembly files within the same directories (e.g. `kernel/entry.c` alongside `kernel/entry.asm`).
- **`LDPP_C` Macro**: Define this compiler switch to bypass the traditional 16-bit real-mode assembly boot blocks, enable the hybrid 1MB memory mapping layout, and activate direct register/GPIO accessing.
- To compile the C hybrid core files using modern GCC or Clang on Linux/Windows:
  ```bash
  gcc -DLDPP_C -O2 -I./hdr -c kernel/entry.c
  ```
  *(Compiles the flat C entry router without 16-bit assembly constraints).*

