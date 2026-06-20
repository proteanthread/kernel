/*
 * asmsupt.c - Optimized Memory & String operations (C17 standard)
 * Original assembly source: asmsupt.asm
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
