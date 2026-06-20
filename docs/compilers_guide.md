# LibreDOS Kernel Compilers & Build Systems Guide

This document describes how to compile the LibreDOS (`DOS-C`) kernel codebase under various target architectures, compilers, and host operating systems.

---

## 1. Supported Toolchains & Platforms

The kernel repository supports compilation under multiple compilers, target processors, and hosting operating systems.

### A. LibreDOS (OpenWatcom)
OpenWatcom is the preferred and recommended compiler for targeting 16-bit real mode.
- **Compiler**: `wcc` (16-bit C compiler), `wcl` (linker).
- **Assembler**: `wasm` or `nasm` (Netwide Assembler).
- **Host System**: DOS, Windows, or Linux (cross-compiler binaries).
- **Compilation Commands**:
  - Load environment variables using `config.bat` (copied from `config.b`).
  - Run `build.bat wc 386` (to build for 80386 CPU target with OpenWatcom).
  - Watcom files use memory models tailored to segmented architectures (specifically compiling in small/medium models with code in segment `INIT_TEXT` / `HMA_TEXT` and data grouped under `DGROUP`).

### B. Windows 10 / 11 (Microsoft Visual C++)
Compiling the kernel on modern Windows using MSVC requires understanding the shift in compiler capability. Modern MSVC (VS 2017/2019/2022) does not support 16-bit real-mode x86 code emission.
- **Build Configurations**:
  - **Modern 64-bit Targets**: To support 64-bit modernization (UEFI booting), MSVC is used in x64 mode. Inline assembly `__asm` blocks must be removed or moved to external `.asm` files compiled with MASM (`ml64.exe`), as MSVC x64 does not support inline assembly.
  - **16-bit Emulation / Legacy**: For compiling 16-bit components, a legacy MSVC toolset (Microsoft C/C++ Compiler 8.0, target of `mscl8.mak`) or a cross-compilation pipeline (OpenWatcom for Windows) is invoked.
  - **Assembly Translation**: Segment qualifiers like `__segment` (used in [main.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/main.c) for MSVC segments linkage) are resolved by grouping code into standard segments and declaring variables within explicit linker sections (`#pragma section`).

### C. Linux (GCC - ia16-elf-gcc)
Cross-compiling on Unix/Linux hosts is fully supported using T.K. Chia's `ia16-elf-gcc` compiler fork (a version of GCC specifically targeting 16-bit Intel x86 Real Mode).
- **Toolchain Components**: `ia16-elf-gcc` (compiler), `ia16-elf-ld` (linker), `nasm` (assembler).
- **Build Steps**:
  1. Copy `config.m` to `config.mak` and edit compiler tool paths.
  2. Run `make all COMPILER=gcc` to execute compile.
  3. The compilation pipeline relies on [kernel.ld](file:///C:/Users/rtdos/GitHub/kernel/kernel/kernel.ld) linker script to map the segments (such as positioning the `PSP` scratchpad at virtual offset zero and aligning `INIT_TEXT` and `HMA_TEXT` to paragraph boundaries).

---

## 2. Core Build Scripts & Configurations

The build pipeline is controlled by standard configuration files and driver batch scripts:

### A. Configuration Templates
- **`config.b` (DOS/Windows)**: Defines environment variables such as:
  - `COMPILER`: Target compiler (WATCOM, TC2, MSCL8, etc.).
  - `XCPU`: Processor architecture optimization target (`86`, `186`, `386`).
  - `XFAT`: Filesystem target support (`16` or `32`).
  - `UPX`: Path to the UPX binary compressor executable.
- **`config.m` (Linux)**: Defines equivalent variables for GNU Make toolchains (`CC`, `AS`, `LD`, `XFAT`, `XCPU`).

### B. Core Build Executables
- **`build.bat`**: The central orchestrator batch file. It cleans, configures compiler switches, and builds dependencies in order:
  1. `utils`: Builds compilation tools (`exeflat`, `patchobj`, `relocinf`).
  2. `lib`: Compiles helper libraries.
  3. `drivers`: Compiles low-level BIOS drivers.
  4. `boot`: Builds the partition and disk boot sector files.
  5. `sys`: Compiles the system transfer installer (`SYS.COM`).
  6. `kernel`: Links the kernel binary (`KERNEL.SYS`).
  7. `country`: Compiles locale uppercase and sorting databases.
  8. `setver`: Compiles version replacement driver.
- **`buildall.bat`**: Sweeps through all compiler and CPU targets sequentially to check code build integrity.
- **`clean.bat` / `clobber.bat`**: Standard maintenance scripts to delete intermediary `.obj` / `.o` files and compiled targets (`KERNEL.SYS`, `SYS.COM`, etc.).

---

## 3. Binary Compression (UPX)

To reduce disk space requirements, the kernel and utilities can be post-processed using the UPX packer:
- **Command**: `upx --8086 --best bin\sys.com` or `upx --8086 KERNEL.SYS`.
- **Loader support**: When the kernel is compressed via UPX, it can no longer boot directly using standard binary entry. A custom decompressor header (assembled from [upxentry.asm](file:///C:/Users/rtdos/GitHub/kernel/utils/upxentry.asm) and [upxdevic.asm](file:///C:/Users/rtdos/GitHub/kernel/utils/upxdevic.asm)) is prepended to the binary. During boot, this header executes first, decompresses the main kernel code block in memory, and jumps to the original execution offset.
