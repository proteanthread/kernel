# LibreDOS Core API & Integration Guide (`API.md`)

This guide is the master technical reference for developers, system programmers, and integration engineers targeting **LibreDOS**. It documents the APIs exposed by the C Core, bare-metal hardware hooks, IoT platform buses, and emulator setups (specifically for **86Box**).

---

## 1. LibreDOS Memory Specification (LMS) APIs

These C17 functions manage memory contexts, dynamic paging, and memory mapping translation boundaries within the LMS framework.

### A. Segment Translation
```c
inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk);
```
- **Description**: Translates standard 16-bit real-mode `Segment:Offset` logical pointers into a flat, 64-bit physical address mapped inside the chunk's allocated space.
- **Parameters**:
  - `segment`: The 16-bit segment register value.
  - `offset`: The 16-bit offset value.
  - `chunk`: Pointer to the target virtual machine `dos_chunk_t` context.
- **Returns**: A `void*` pointer targeting the physical memory offset on the host machine.

### B. Dynamic Page Allocation (BSD-Style Pager)
```c
void* lms_allocate_pages(uint32_t num_pages);
void  lms_free_pages(void *ptr, uint32_t num_pages);
```
- **Description**: Requests or releases contiguous physical memory page frames (4KB each) from the dynamic host physical allocator (DLA).
- **Parameters**:
  - `num_pages`: Number of contiguous 4KB blocks requested.
  - `ptr`: Base memory pointer to deallocate.
- **Returns** (`lms_allocate_pages`): A `void*` pointer to the allocated block, or `NULL` if allocation fails.

### C. LMS Context Initialization
```c
void lms_init_system_chunk(uint32_t system_id, uint8_t *host_base_address);
```
- **Description**: Configures a sandbox context descriptor (`dos_chunk_t`) for a virtual system or VM slot.
- **Parameters**:
  - `system_id`: Unique identifier index (0 to 15).
  - `host_base_address`: Pointer mapping the start of the 1MB logical RAM block.

---

## 2. Emulated Interrupt Service Routers

These handlers process software interrupts, enabling legacy real-mode drivers and applications to request system resources.

### A. Primary DOS Services Router (Int 21h)
```c
void ld_int21_service(regs_context_t *regs, dos_chunk_t *chunk);
```
- **Description**: Intercepts DOS requests (file actions, process launches, allocations).
- **LMS Allocations (AH=58h)**:
  - `AL=02h`: Get UMB link status. Returns `AX=1` if Upper Memory Blocks are linked.
  - `AL=03h`: Link/Unlink UMB space. Links upper segment buffers into the conventional MCB list.

### B. Virtual XMS Handler (Int 2Fh AX=4310h)
```c
void lms_xms_service(regs_context_t *regs, dos_chunk_t *chunk);
```
- **Description**: Services extended memory requests (originally HIMEM.SYS functions):
  - **Func 08h**: Query Free Extended Memory. Returns sizes in KB to registers `AX` and `DX`.
  - **Func 09h**: Allocate EMB. Dynamically allocates host pages and returns an XMS handle in `DX`.
  - **Func 0Ah**: Free EMB. Deallocates host pages registered to the given handle.
  - **Func 0Bh**: Move EMB. Moves blocks between conventional memory segments and XMS handles using C `memcpy`.

### C. Virtual LIM EMS 4.0 Handler (Int 67h)
```c
void lms_ems_service(regs_context_t *regs, dos_chunk_t *chunk);
```
- **Description**: Services expanded memory requests (originally EMM386 functions) via page mapping:
  - **Func 41h**: Get Page Frame Segment. Returns `AX=0000` and `BX=D000` (segment mapping offset).
  - **Func 43h**: Allocate Handle and Pages. Allocates page groups (16KB each) from the host pool.
  - **Func 44h**: Map Page. Bank-switches logical handle page pages to physical frame windows 0 to 3.

---

## 3. Bare Metal & IoT Platform Interfaces

For hardware platforms lacking standard PC BIOS components (microcontrollers and SBCs), the kernel maps character and block devices to hardware controllers.

```
       +---------------------------------------------+
       |             LibreDOS C Core                 |
       +----------------------+----------------------+
                              |
         +--------------------+--------------------+
         v (Console Redirection)                   v (Storage Adapter)
  +--------------+                           +--------------+
  |  UART Serial |                           | SPI SD Card  |
  +--------------+                           +--------------+
```

### A. Console Redirection (CON)
- **UART Port Mapping**:
  - Microcontrollers (like ESP32/Arduino) map character output in `chario.c` to serial UART register offsets.
  - Raspberry Pi targets map output to the Mini UART registers (`0x3F215040`).
- **UEFI Framebuffer (GOP)**:
  - Real machines booting natively via UEFI write console output directly to the EFI Graphics Output Protocol linear framebuffer, bypassing legacy BIOS `Int 10h` calls.

### B. SPI / GPIO Driver Wrappers
- **SPI Bus access**:
  - Block sector reads in `dsk.c` are routed to direct SPI adapters when booting on Arduino/ESP32, converting block read/write parameters into SPI commands to read SD/MMC cards.
- **GPIO Pin Controls**:
  - Hardware status registers are checked by hooking auxiliary interrupt vectors to read GPIO lines.

---

## 4. Emulator Integration: 86Box Setup Guide

**86Box** is a hyper-accurate emulator that replicates real x86 motherboard chipsets, ISA/PCI expanders, and micro-architectures. Testing LibreDOS on 86Box ensures 100% PC-DOS and MS-DOS compatibility.

### A. Recommended Virtual Machine Configurations
To run the compiled LibreDOS kernel and verify all memory specifications:

1. **Machine Selection**:
   - **Category**: [Socket 7]
   - **Model**: [i430VX] Asus P/I-P55T2P4 or [i430TX] Gigabyte GA-5AX.
   - **CPU**: Intel Pentium 75 to 133 MHz. (Allows testing protected-mode instructions and virtual drivers).
2. **Memory Configuration**:
   - **Conventional RAM**: 640 KB.
   - **Extended Memory (XMS)**: Set between 16 MB and 128 MB to verify `HIMEM` routines.
3. **Display Adapters**:
   - **Video Card**: [PCI] S3 Trio64 or Tseng Labs ET4000/W32p.
4. **Storage Controller**:
   - **IDE Controller**: Standard Dual-Channel PCI IDE Controller.
   - **Floppy Drives**: 3.5" 1.44MB floppy mapped to the built-in FDC.

### B. Configuring Memory Drivers for Compatibility Testing
When running LibreDOS on 86Box:

- **HIMEMX / HIMEM**: Loads into the config chain to allocate extended memory via the BIOS Int 15h (E820h/E801h) services emulated by 86Box.
- **EMM386**: Configured with `RAM` parameter to map the LIM EMS 4.0 page frame in the unused UMA region:
  ```ini
  DEVICE=HIMEM.EXE
  DEVICE=EMM386.EXE RAM D000-DFFF UMB
  DOS=HIGH,UMB
  ```
  - This instruction maps UMBs between `C8000h` and `EFFFFh`, reserving `D0000h - DFFFFh` for the EMS Page Frame.
- **SYS CONFIG Configuration**:
  - Customize the compiled kernel file using `fdkrncfg.c` settings to verify load-high configurations inside 86Box.
