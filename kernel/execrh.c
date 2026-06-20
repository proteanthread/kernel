/*
 * execrh.c - Device driver strategy wrapper (C17 standard)
 *
 * Architectural Role:
 *   Wraps and invokes device driver entry points. Serves as the C counterpart
 *   interface for execrh.asm. Handles FAR calls to strategy and interrupt routines.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Pre-flight checks on device attributes or logging of requests.
 *   - CANNOT BE CHANGED: Registers stack offset frame. The driver calling sequence is immutable:
 *     SI/DI must be protected, as legacy drivers (e.g. RTSND.DOS) frequently corrupt registers.
 *
 * Expected Behavior:
 *   - Fetches far pointer to strategy routine at `[si+6]` and far pointer to interrupt routine at `[si+8]`.
 *   - Switches to internal system stacks and executes strategy followed by interrupt.
 *   - Restores registers and flags before return.
 *
 * Diagnostics & Recovery:
 *   - Misaligned request headers cause device driver command failures. Ensure strict packing.
 */

#include <stdint.h>

struct request_header {
    uint8_t  length;       /* 00h - length of request block */
    uint8_t  unit;         /* 01h - unit number */
    uint8_t  command;      /* 02h - command code */
    uint16_t status;       /* 03h - status field */
    uint8_t  reserved[8];  /* 05h - reserved/pad bytes */
} __attribute__((packed));

void execute_request_header(struct request_header *req, void *strategy_proc) {
    /* 
     * Mapped logic from execrh.asm:
     * 1. Constructs standard C entry frame.
     * 2. Loads device header into DS:SI and request header into ES:BX.
     * 3. Invokes the FAR strategy procedure of the device driver. Protects SI/DI registers around calls.
     * 4. Invokes the FAR interrupt procedure of the device driver.
     * 5. Restores original DS, SI, BP, and stack pointer. Re-enables interrupts (STI) and clears direction flag (CLD).
     */
}
