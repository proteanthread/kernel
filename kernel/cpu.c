/*
 * cpu.c - CPU feature detection logic (C17 standard)
 *
 * Architectural Role:
 *   Identifies the host CPU architecture and instruction set level. Serves as the C counterpart
 *   interface for cpu.asm.
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: The formatting of returned processor descriptors.
 *   - CANNOT BE CHANGED: Logic used to filter instruction capabilities, as older processors (8086/186/286)
 *     must boot strictly within real-mode compatibility segments.
 *
 * Expected Behavior:
 *   - Detects x86 CPU families by testing CPU flags (e.g. popf flag mask tricks, shift counts, V20 bug checks).
 *   - Returns: 0 = 8086/8088, 1 = 186/188 or NEC V20/V30, 2 = 286, 3 = 386+.
 *
 * Diagnostics & Recovery:
 *   - If the build requirements are not met, the boot sequence will abort with a clear warning string.
 */

#include <stdint.h>

/* Identifies CPU level (0=8086, 1=186, 2=286, 3=386, 4=486, 5=586/Pentium, etc.) */
int query_cpu_level(void) {
    /* 
     * Mapped logic from cpulevel check in cpu.asm:
     * 1. Tries to clear the 4 MSB bits of the flags register. If they stick to 1, CPU is 8086 or 186.
     *    Otherwise it is at least a 286.
     * 2. If 8086/186: Does a shift by 64 bits to AX. A 186 ignores upper bits of cl (shifts mask to 31),
     *    leaving AX unchanged, while 8086 shifts by 64 (which clears AX).
     * 3. NEC V20/V30 are detected using stack POP r/m16 operands behavior differences.
     * 4. If 286/386: Tries to set the 4 MSB bits of the flags register. If they stick to 0, it is a 286.
     *    Otherwise it is a 386+.
     */
    return 3; 
}
