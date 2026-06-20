# LibreDOS Modern Driver Integration Tutorial

This tutorial describes how to integrate modern and classic hardware device drivers into the LibreDOS kernel. The driver model is designed to behave monolithically with clean module attachment (similar to BSD Unix), supporting modern **UEFI-based systems** (which lack legacy BIOS interrupts) and **IoT platforms** (ESP32, Arduino, Raspberry Pi), while preserving 100% compatibility with MS-DOS/PC-DOS applications.

---

## 1. The Architectural Shift: UEFI & IoT vs. Legacy BIOS

Traditional DOS drivers rely on real-mode x86 BIOS interrupts (e.g. `Int 13h` for disk operations, `Int 10h` for screen output, `Int 14h` for serial communication). On modern UEFI systems and IoT platforms, **these BIOS interrupts do not exist**. 

### UEFI Challenges:
- **No MBR / Legacy Boot**: UEFI boots the system in 32-bit or 64-bit protected mode. There is no Real Mode execution space or standard Segment-Offset memory structure.
- **No Interrupt Vectors**: Accessing disks or consoles requires querying UEFI Runtime Services or directly mapping and writing to Memory-Mapped I/O (MMIO) hardware registers (e.g. PCIe configuration registers or the graphics framebuffer).

### IoT Challenges:
- **Architecture Portability**: Boards like the ESP32 (Xtensa), Arduino (AVR), and Raspberry Pi (ARM) have no x86 processor instructions.
- **Direct Bus Communication**: Drivers must read and write directly to hardware pins and controllers via interfaces like **GPIO**, **SPI**, **I2C**, and **UART**.

---

## 2. The BSD Unix-Inspired Driver Model

To support both classic and modern platforms, LibreDOS adopts a unified, structured driver registration system inspired by BSD Unix autoconfiguration.

```
       +---------------------------------------------+
       |             LibreDOS File System            |
       |         (dosfns.c / SFT / File Handles)     |
       +----------------------+----------------------+
                              |
                              v
       +----------------------+----------------------+
       |          DOS Device Driver Chain            |
       |   (Link list starting at LoL->nul_dev)      |
       +----------------------+----------------------+
                              |
                              v  Device Wrapper / Trampoline
       +----------------------+----------------------+
       |           Unified Driver Core               |
       |  (Dynamic Module registration & probe)      |
       +----------+-----------------------+----------+
                  |                       |
                  v                       v
       +----------+----------+ +----------+----------+
       |   UEFI Video Driver | |  IoT SPI Card Reader |
       |   (Direct MMIO FB)  | |  (Direct GPIO pins)  |
       +---------------------+ +---------------------+
```

### Driver Interface Structure:
Drivers are defined by a standard configuration structure instead of raw assembly headers:

```c
struct driver {
    const char *name;
    int  (*probe)(struct bus *parent);
    int  (*attach)(struct device *self);
    int  (*detach)(struct device *self);
    long (*read)(struct device *self, void *buf, unsigned long len);
    long (*write)(struct device *self, const void *buf, unsigned long len);
    int  (*ioctl)(struct device *self, unsigned int cmd, void *arg);
};
```

During boot, the kernel probes the active hardware bus (PCI, SPI, or UEFI runtime descriptors) and invokes the driver's `probe()` and `attach()` functions to configure memory and registers.

---

## 3. Memory Allocation: Where to Load Drivers

In a dynamic memory kernel model, driver binaries are not restricted to fixed, low-memory conventional addresses:

### A. High Memory Area (HMA) and Flat Heap
- Drivers are compiled as relocatable object files.
- During system boot or dynamic loading (`LOAD=driver.sys`), the kernel allocates space from a **System Driver Space Heap** located in the High Memory Area (above the 1MB boundary) or in high physical memory mapped via UEFI page tables.

### B. Segment Alignment & Memory Protection
- Because modern processors support memory protection, the dynamic driver memory space should be configured with read/execute (`R-X`) permissions for code and read/write (`RW-`) permissions for data. This prevents drivers from corrupting the core kernel memory space.

---

## 4. Preserving MS-DOS & PC-DOS Compatibility

While the drivers execute in flat, high-memory spaces (often 32-bit or 64-bit), legacy DOS programs require 16-bit real-mode structures. To maintain full compatibility:

### A. The Trampoline Device Wrapper
For each loaded driver, the kernel reserves a small 32-byte stub in conventional memory (below 640KB) and inserts it into the traditional `LoL->nul_dev` linked list. 

This stub contains:
1. **Device Attribute Word**: Configured to match legacy attributes (e.g. Character device, CONIN, CONOUT).
2. **Strategy Pointer**: Points to a small assembly trampoline.
3. **Interrupt Pointer**: Points to a small assembly trampoline.

```assembly
strategy_trampoline:
    push    ds
    push    es
    ; Switch stack to System Driver stack
    ; Call high-memory driver entry (which may transition to 32/64-bit mode)
    call    far [cs:high_memory_driver_strategy_ptr]
    pop     es
    pop     ds
    retf
```

### B. Address Translation for Legacy Memory Pointers
- Legacy DOS programs pass data buffers using 16-bit `Segment:Offset` pointers.
- When the trampoline routes execution to the modern driver, the translation layer must convert these segmented pointers into flat linear addresses (e.g., `(Segment << 4) + Offset`) before the driver performs reads or writes.
- For DMA (Direct Memory Access) operations (such as disk reads to conventional DTAs), the driver must use a **Bounce Buffer** in conventional memory if the physical controller cannot address memory above the 1MB or 4GB boundaries.

---

## 5. UEFI and Embedded IoT Driver Implementation

### A. UEFI Video Driver Implementation (FrameBuffer)
On systems booting via UEFI, the legacy VGA write routines must be replaced.
1. The UEFI bootloader queries the `EFI_GRAPHICS_OUTPUT_PROTOCOL` (GOP) to locate the physical address of the video framebuffer and active resolution parameters.
2. These parameters are passed to the kernel in a boot block structure.
3. The video driver maps this framebuffer address into memory and implements console writes directly by drawing character glyph bitmaps onto the pixel screen, completely bypassing BIOS `Int 10h`.

### B. IoT ESP32 / Raspberry Pi Driver Implementation (GPIO/SPI)
For microcontroller and single-board computer integration:
1. The driver directly reads and writes to physical registers mapped to the controller's bus.
2. *Example (SPI SD Card Reader on ESP32)*:
   - The block driver initializes the hardware SPI controller using direct register offsets.
   - It registers itself as a block device.
   - When the kernel requests sector transfers (`dskxfer`), the driver serializes commands across the SPI pins to fetch data blocks from the SD card.
   - It copies the blocks to the bounce buffer, allowing the DOS filesystem layer to read partition sectors seamlessly.
