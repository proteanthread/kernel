/*
 * execrh.c - Device driver strategy wrapper (C17 standard)
 * Original assembly source: execrh.asm
 */

#include <stdint.h>

struct request_header {
    uint8_t  length;
    uint8_t  unit;
    uint8_t  command;
    uint16_t status;
    uint8_t  reserved[8];
};

void execute_request_header(struct request_header *req, void far *strategy_proc) {
    /* Executes device strategy entry */
}
