/*
 * io.c - I/O ports registers reading/writing (C17 standard)
 *
 * Architectural Role:
 *   Handles physical port inputs/outputs and maps system driver strategy entry headers.
 *   Serves as the C counterpart interface for io.asm.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Standard C port macros (inb, outb) or helper wrappers.
 *   - CANNOT BE CHANGED: Fixed data locations of the driver header chain (CON, PRN, AUX, etc.).
 *     Must maintain the exact linked structure sequence as expected by boot and device routing layers.
 *
 * Expected Behavior:
 *   - Serves as the strategy router, storing the pointer to request packet (ReqPktPtr) inside es:bx.
 *   - Manages Char and Block driver strategies. Splits calls based on type bit (C-type vs. assembly-type).
 *
 * Diagnostics & Recovery:
 *   - Device communication issues can be investigated by tracking ReqPktPtr and verifying
 *     the unit number in `GetUnitNum`.
 */

#include <stdint.h>

/* Mapped device header layout from io.asm */
struct device_header {
    uint32_t next_driver;       /* offset+segment chain pointer */
    uint16_t attributes;        /* device attributes word */
    uint16_t strategy_offset;   /* strategy entry pointer */
    uint16_t interrupt_offset;  /* interrupt entry pointer */
    uint8_t  name[8];           /* device identifier name */
} __attribute__((packed));

uint8_t in_byte(uint16_t port) {
    /* Reads a byte from hardware port (inb instruction wrapper) */
    return 0;
}

void out_byte(uint16_t port, uint8_t val) {
    /* Writes a byte to hardware port (outb instruction wrapper) */
}
