/*
 * nlssupt.c - NLS conversion wrappers (C17 standard)
 * Original assembly source: nlssupt.asm
 */

#include <stdint.h>

char uppercase_character(char c) {
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
}
