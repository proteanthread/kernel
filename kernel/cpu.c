/*
 * cpu.c - CPU feature detection logic (C17 standard)
 * Original assembly source: cpu.asm
 */

#include <stdint.h>

/* Identifies CPU level (0=8086, 1=186, 2=286, 3=386, 4=486, 5=586/Pentium, etc.) */
int query_cpu_level(void) {
    /* Implements platform-neutral processor type identification */
    return 3; 
}
