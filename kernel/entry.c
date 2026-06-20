/*
 * entry.c - System API Interrupt Router & Context Entry (C17 standard)
 * Original assembly source: entry.asm
 */

#include <stdint.h>

#define CHUNK_SIZE_1MB 1048576

typedef struct {
    uint32_t AX, BX, CX, DX;
    uint32_t SI, DI, BP, SP;
    uint16_t DS, ES, FS, GS, SS, CS;
    uint32_t Flags;
} regs_context_t;

typedef struct {
    uint8_t *base_address;    /* 64-bit physical chunk offset pointer */
    uint32_t chunk_size;      /* 1MB segment size limit */
} dos_chunk_t;

/* Translate Segment:Offset address bounds to 64-bit Flat memory pointer */
inline void* ld_translate_address(uint16_t segment, uint16_t offset, dos_chunk_t *chunk) {
    uint32_t linear_offset = ((uint32_t)segment << 4) + offset;
    return chunk->base_address + (linear_offset % CHUNK_SIZE_1MB);
}

/* Routes system call interrupts */
void ld_int21_service(regs_context_t *regs, dos_chunk_t *chunk) {
    uint8_t ah = (regs->AX >> 8) & 0xFF;
    switch (ah) {
        case 0x4C: /* Exit */
            break;
        default:
            break;
    }
}
