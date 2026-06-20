/*
 * apisupt.c - API Support helper routines (C17 standard)
 * Original assembly source: apisupt.asm
 */

#include <stdint.h>

/* Resolves memory address offsets for systems calls */
uint32_t get_user_offset(uint16_t segment, uint16_t offset) {
    return ((uint32_t)segment << 4) + offset;
}
