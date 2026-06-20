# LibreDOS Kernel User's Guide

This document describes how to configure, deploy, and adjust parameters for the LibreDOS (`DOS-C`) kernel.

---

## 1. SYS CONFIG Command Tool

The LibreDOS kernel embeds configuration parameters inside its executable binary (mapped to the `_LowKernelConfig` struct in [kernel.asm](file:///C:/Users/rtdos/GitHub/kernel/kernel/kernel.asm)). Users can inspect and modify these parameters directly without recompiling, using the `SYS CONFIG` command of [SYS.COM](file:///C:/Users/rtdos/GitHub/kernel/sys/sys.c).

### Core Syntax:
- **Inspect Default**: `SYS CONFIG` (looks for `KERNEL.SYS` in current directory).
- **Inspect Specific File**: `SYS CONFIG [path]\KERNEL.SYS`
- **Modify Parameters**: `SYS CONFIG [path]\KERNEL.SYS OPTION1=value [OPTION2=value]`

### Supported Options:
- **`DLASORT`** (`0` or `1`):
  - `0` (Default): Standard MS-DOS Drive Letter Assignment. All primary partitions on all physical drives are lettered first, followed by extended/logical partitions.
  - `1`: Drive-by-Drive Assignment. Maps all partitions (primary and logical) on the first drive before letter mapping moves to the next physical disk.
- **`SHOWDRIVEASSIGNMENT`** (`0` or `1`):
  - `1` (Default): Displays partition layouts and drive letters on screen during startup.
  - `0`: Suppresses drive letter mapping tables during boot.
- **`SKIPCONFIGSECONDS`** (`-128` to `127`):
  - `> 0`: Number of seconds the kernel waits for the user to press `F5` (skip config) or `F8` (single-step trace) during boot.
  - `0`: Bypasses timeout check, moving directly to configuration parsing.
  - `< 0`: Disables `F5`/`F8` configuration bypass key detection.
- **`FORCELBA`** (`0` or `1`):
  - `1`: Forces the kernel to use INT 13h LBA (Logical Block Addressing) extensions to access drives, bypassing CHS boundaries.
  - `0` (Default): Standard partition compatibility logic.
- **`GLOBALENABLELBASUPPORT`** (`0` or `1`):
  - `1` (Default): Enables disk LBA support.
  - `0`: Disables LBA support (forces CHS modes for legacy BIOSes).

*Example Command*:
```cmd
SYS CONFIG C:\KERNEL.SYS SKI=5 SHOWDRIVEASSIGNMENT=0
```
*(Sets configuration timeout to 5 seconds and hides drive lettering tables).*

---

## 2. CONFIG.SYS Configuration Directives

During the boot phase, the kernel reads `CONFIG.SYS` from the root of the boot drive. Directives are parsed sequentially in [config.c](file:///C:/Users/rtdos/GitHub/kernel/kernel/config.c):

- **`BUFFERS=N`**: Allocates the number of disk sector cache blocks (typically `10` to `40`). If DOS is loaded high (`DOS=HIGH`), these blocks are allocated inside the High Memory Area (HMA).
- **`FILES=N`**: Allocates the maximum number of simultaneously open system-wide file handles.
- **`LASTDRIVE=Letter`**: Restricts the maximum drive letter mapping bounds (e.g. `LASTDRIVE=Z`).
- **`SHELL=[path] [args]`** (or `INIT=[path]`): Defines the primary command interpreter shell (typically `C:\COMMAND.COM /P /E:1024`).
- **`COUNTRY=code,[page],[file]`**: Activates National Language Support for character sorting, lowercase conversion, and date formats. Loads NLS catalogs dynamically from file (typically `COUNTRY.SYS`).
- **`DOS=HIGH|LOW,UMB|NOUMB`**:
  - `HIGH`: Relocates resident kernel elements to the High Memory Area (HMA).
  - `UMB`: Links Upper Memory Blocks (UMBs) created by memory managers (like EMM386) to the DOS memory allocator pool.
- **`DEVICE=[path]\driver.sys`**: Loads a character or block device driver into conventional memory.
- **`DEVICEHIGH=[path]\driver.sys`**: Loads a device driver into Upper Memory Blocks (above 640KB) to preserve conventional space.
- **`INSTALL=[path]\program.exe`**: Executing TSR (Terminate and Stay Resident) utilities during driver initialization.
- **`INSTALLHIGH=[path]\program.exe`**: Loads TSR utilities into Upper Memory Blocks.

---

## 3. Memory Configurations

To make the best use of memory, the kernel supports relocation configurations:

- **HMA Routing**: Relocating code to the High Memory Area (HMA) clears approximately 40KB to 50KB of conventional space. If `DOS=HIGH` is specified, the startup code enables the A20 line and copies the resident kernel segments `HMA_TEXT` and cache buffers directly to the `FFFFh` segment.
- **UMB Routing**: By using memory managers to map physical ROM gaps between 640KB and 1MB (e.g. `D000:0000`), the DOS allocator hooks these regions into the MCB chain. Programs loaded using `LH` (LoadHigh) or drivers loaded via `DEVICEHIGH` are allocated inside these UMBs.
