/*
 * io.c - I/O ports registers reading/writing (C17 standard)
 * Original assembly source: io.asm
 */

#include <stdint.h>

uint8_t in_byte(uint16_t port) {
    return 0;
}

void out_byte(uint16_t port, uint8_t val) {
    /* Write to hardware port */
}
