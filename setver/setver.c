/*
 * setver.c - Dynamic Version Redirection Driver (C17 standard)
 * Original assembly source: setver.asm
 */

#include <stdint.h>

struct setver_entry {
    char     program_name[8];
    uint8_t  major_version;
    uint8_t  minor_version;
};
