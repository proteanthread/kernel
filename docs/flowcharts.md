# LibreDOS Kernel Flowcharts

This document provides visual flowcharts of key execution paths within the LibreDOS kernel using Mermaid syntax.

---

## 1. Kernel Boot Phase Sequence

This diagram maps the boot phase execution flow starting from the BIOS MBR bootstrap loader through the execution of `COMMAND.COM`.

```mermaid
graph TD
    A[BIOS Bootstrap / MBR] -->|Loads boot.asm / boot32.asm| B(Boot Sector Execution)
    B -->|Loads KERNEL.SYS to 0060h:0000h| C(kernel.asm: entry)
    C --> D{CPU Level Check}
    D -->|Unsupported CPU| E[Print Error & Halt/Reboot]
    D -->|Supported CPU| F(Relocate INIT segment to High Memory)
    F --> G{Is HMA Active?}
    G -->|Yes| H(Relocate HMA_TEXT to FFFFh:0000h)
    G -->|No| I(Keep Resident Code in Conventional Memory)
    H --> J(far call to main.c: LibreDOSmain)
    I --> J
    J --> K[setup_int_vectors]
    K --> L[init_kernel]
    L --> M[InitIO - Load Character Devices]
    M --> N[DoConfig - Parse CONFIG.SYS passes 0, 1, 2]
    N --> O[PostConfig - Allocate sector buffers & SFTs]
    O --> P[InitializeAllBPBs]
    P --> Q[kernel - Run COMMAND.COM]
```

---

## 2. Int 21h System Call Handling

This diagram details the path of an `Int 21h` system call, demonstrating stack shifting, re-entrancy prevention, and execution routing.

```mermaid
graph TD
    A[User Program: int 21h] --> B(entry.asm: reloc_call_int21_handler)
    B --> C{AH System Call Type?}
    C -->|Re-entrant API: 33h, 50h, 51h, 62h| D[int21_syscall]
    C -->|Standard DOS API| E[Push All Registers]
    D -->|Execute on User Stack| F[Return via iret]
    E --> G[Switch DS to DGROUP]
    G --> H{Is InDOS or ErrorMode Active?}
    H -->|Yes: Error Stack| I[Switch SS:SP to error_tos stack]
    H -->|No: Standard Stack| J{AH function class?}
    J -->|Console/Char I/O| K[Switch SS:SP to char_api_tos stack]
    J -->|Disk/Other operations| L[Switch SS:SP to disk_api_tos stack]
    I --> M[Call inthndlr.c: int21_service]
    K --> M
    L --> M
    M --> N[Map AH to service handlers]
    N --> O[Restore original SS:SP from backup]
    O --> P[Pop All Registers]
    P --> Q[Return to User Program via iret]
```

---

## 3. FAT / exFAT File System I/O Pipeline

This diagram shows how data flows through a file read or write request and branches between standard FAT and exFAT handling.

```mermaid
graph TD
    A[File Read / Write Request] --> B(dosfns.c: DosRWSft)
    B --> C{Verify Device type}
    C -->|Character Device| D[Call Character I/O Handler: chario.c]
    C -->|Block Device / File| E[Query SFT for Drive Parameter Block]
    E --> F{Check File System Type}
    F -->|FAT12 / FAT16 / FAT32| G[fatfs.c: rwblock]
    F -->|exFAT| H[exfat.c: exfat_rwblock]
    G --> I{Is Block Cached?}
    I -->|Yes| J[Retrieve from Sector Buffer: blockio.c]
    I -->|No| K[dskxfer - Issue BIOS Int 13h Disk Transfer]
    K --> J
    H --> L{Is File Contiguous?}
    L -->|Yes: exFAT contiguous flag| M[Calculate Sector Offset directly: Sector = Start + Offset]
    L -->|No: exFAT allocation bitmap| N[Lookup exFAT Bitmap to locate clusters]
    M --> O[Issue BIOS Int 13h Disk Transfer]
    N --> O
```

---

## 4. Memory Manager MCB Traversal

This diagram details the memory allocation flow through Memory Control Blocks (MCBs) when a program issues an allocation request (AH=48h).

```mermaid
graph TD
    A[DosMemAlloc Request] --> B(memmgr.c: Walk MCB Chain)
    B --> C[Verify MCB Signatures: 4Dh or 5Ah]
    C -->|Signature Mismatch| D[Return Arena Thrashed Error]
    C -->|Valid Signatures| E{Check Allocation Mode}
    E -->|First Fit| F[Scan chain, allocate first block of sufficient size]
    E -->|Best Fit| G[Scan entire chain, locate block closest in size]
    E -->|Last Fit| H[Scan chain backwards, allocate from end of memory]
    F --> I{Block found?}
    G --> I
    H --> I
    I -->|No| J[Return Out of Memory & Largest Free Block Size]
    I -->|Yes| K{Block is exactly required size?}
    K -->|Yes| L[Mark block MCB owner as caller PSP]
    K -->|No| M[Split block: Create new MCB header for remaining memory]
    M --> L
    L --> N[Return segment address of allocated block]
```
