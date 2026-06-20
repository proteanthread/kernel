/*
 * LibreDOS Kernel - C17 Assembly counterpart interface for API Support
 *
 * Architectural Role:
 *   Serves as the C-level counterpart stub for apisupt.asm, providing helper functions
 *   to resolve memory offsets during API system calls.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Logical calculation formatting or local helpers.
 *   - CANNOT BE CHANGED: Memory translation layouts matching registers stack frames in entry.asm/apisupt.asm.
 *
 * Expected Behavior:
 *   - Returns exact byte-offset calculations mapping segment-offset pairs.
 *
 * Diagnostics & Recovery:
 *   - Verify compiler registers usage output against generated assembly file listings.
 */

#include <stdint.h>

/* Resolves memory address offsets for systems calls */
uint32_t get_user_offset(uint16_t segment, uint16_t offset) {
    return ((uint32_t)segment << 4) + offset;
}
