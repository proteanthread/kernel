/*
 * LibreDOS Kernel - C17 Assembly counterpart interface for String & Memory Operations
 *
 * Architectural Role:
 *   Serves as the C-level counterpart stub for asmsupt.asm, implementing wrappers around
 *   low-level memory block copies, compares, and set routines.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Integration of standard compiler intrinsics or optimizations.
 *   - CANNOT BE CHANGED: Function signatures mapped to low-level assembly callers in kernel.
 *
 * Expected Behavior:
 *   - Performs standard string and block operations conforming to ANSI/C17 specifications.
 *
 * Diagnostics & Recovery:
 *   - Memory copy boundary bugs can be verified by analyzing stack boundaries and checking
 *     link alignments in the map file.
 */

#include <stdint.h>
#include <string.h>

void* ld_memcpy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

void* ld_memset(void *s, int c, size_t n) {
    return memset(s, c, n);
}

int ld_strcmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}
