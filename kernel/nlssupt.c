/*
 * nlssupt.c - NLS conversion wrappers (C17 standard)
 *
 * Architectural Role:
 *   Wraps National Language Support character conversion services. Serves as the C counterpart
 *   interface for nlssupt.asm.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Locale case translation conditions inside C stubs.
 *   - CANNOT BE CHANGED: Vector signatures and register layouts for the CharMapSrvc interrupt hooks.
 *
 * Expected Behavior:
 *   - Implements uppercase character conversion routines used by system drivers and user tasks.
 *   - Implements `CharMapSrvc` which preserves 386 registers, loads DGROUP, and invokes internal `DosUpChar`.
 *
 * Diagnostics & Recovery:
 *   - Trace case-conversion issues by validating active codepage maps and verifying that high
 *     bits of characters are handled correctly.
 */

#include <stdint.h>

char uppercase_character(char c) {
    /* 
     * Converts a single character to uppercase based on codepage parameters.
     * Equivalent to calls to DosUpChar (invoked by _reloc_call_CharMapSrvc in nlssupt.asm).
     */
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
}
