/*
 * floppy.c - Floppy Disk Parameter Table setup (C17 standard)
 * Original assembly source: floppy.asm
 */

#include <stdint.h>

/* Standard BIOS Floppy Disk Parameter Table layout */
struct floppy_param_table {
    uint8_t step_rate_head_unload;
    uint8_t head_load_time_dma;
    uint8_t motor_off_delay;
    uint8_t bytes_per_sector_code;
    uint8_t sectors_per_track;
    uint8_t gap_length;
    uint8_t data_length;
    uint8_t format_gap_length;
    uint8_t format_filler_byte;
    uint8_t head_settle_time;
    uint8_t motor_on_delay;
};
