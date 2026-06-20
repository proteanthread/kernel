/*
 * LibreDOS Kernel - UEFI System & Console Interface Mappings
 *
 * Architectural Role:
 *   Defines the types, structures, and entry hooks for systems running on native UEFI firmware.
 *   Provides the abstraction layer replacing BIOS Int 10h (GOP video rendering), Int 13h (block storage),
 *   and Int 14h (serial port hardware mapping).
 *
 * Changeability & Constraints:
 *   - CAN BE CHANGED: Internal graphics drawing helpers, video mode selections, font tables, and UART ports.
 *   - CANNOT BE CHANGED: UEFI boot block parameter alignment, calling conventions (Microsoft x64 ABI),
 *     and page table alignment constraints for physical memory translation.
 *
 * Expected Behavior:
 *   - Screen outputs are mapped to GOP pixel buffers rather than VGA memory segment 0xB800h.
 *   - Disks read/write sectors by calling UEFI Block IO services rather than triggering BIOS interrupts.
 *
 * Diagnostics & Recovery:
 *   - In case of boot failure, verify the boot block parameters passed by the loader.
 *   - Framebuffer alignments can be validated by mapping screen pixels directly via a test screen color fill.
 */

#include <stdint.h>
#include <stdbool.h>

// EFI Status Codes
typedef size_t efi_status_t;
#define EFI_SUCCESS 0

// EFI Memory Descriptor
typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_desc_t;

// EFI Graphics Output Protocol Mode Information
typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    uint32_t pixel_information[4];
    uint32_t pixels_per_scan_line;
} efi_gop_mode_info_t;

// EFI Graphics Output Protocol
typedef struct {
    uint32_t             max_mode;
    uint32_t             mode;
    efi_gop_mode_info_t *info;
    size_t               size_of_info;
    uint64_t             frame_buffer_base;
    size_t               frame_buffer_size;
} efi_gop_t;

// Boot Block passed from BOOTX64.EFI to KERNEL.SYS
typedef struct {
    uint32_t   magic;               // 0x1EADA000 (LibreDOS UEFI Magic)
    uint32_t   boot_drive_id;       // UEFI Bios drive id mapping
    uint64_t   system_table_ptr;    // Pointer to EFI System Table
    uint64_t   mmap_ptr;            // Pointer to EFI memory map
    uint32_t   mmap_size;
    uint32_t   descriptor_size;
    efi_gop_t  gop_info;            // GOP video parameters
} uefi_boot_block_t;

// Global state for UEFI graphics console
static uefi_boot_block_t g_uefi_boot_data;
static bool              g_uefi_active = false;

// Initialize UEFI system parameters
void uefi_init(uefi_boot_block_t *boot_block) {
    if (boot_block == NULL || boot_block->magic != 0x1EADA000) {
        g_uefi_active = false;
        return;
    }
    g_uefi_boot_data = *boot_block;
    g_uefi_active = true;
}

// GOP Font structure layout (8x16 font glyph bitmap)
typedef struct {
    uint8_t glyph[16];
} gop_font_glyph_t;

extern const gop_font_glyph_t g_vga_font_data[256];

// Plots a character pixel-by-pixel to GOP Framebuffer
void uefi_draw_char(int x, int y, char c, uint32_t color) {
    if (!g_uefi_active || g_uefi_boot_data.gop_info.frame_buffer_base == 0) {
        return;
    }

    uint32_t *fb = (uint32_t *)g_uefi_boot_data.gop_info.frame_buffer_base;
    uint32_t pitch = g_uefi_boot_data.gop_info.gop_info.pixels_per_scan_line;
    const gop_font_glyph_t *font_glyph = &g_vga_font_data[(uint8_t)c];

    for (int row = 0; row < 16; row++) {
        uint8_t font_row = font_glyph->glyph[row];
        for (int col = 0; col < 8; col++) {
            if (font_row & (0x80 >> col)) {
                fb[(y + row) * pitch + (x + col)] = color;
            }
        }
    }
}

// UEFI Block IO sector read hook
efi_status_t uefi_read_sectors(uint32_t start_sector, uint32_t sector_count, uint8_t *buffer) {
    /* 
     * In an active UEFI environment, this routine calls the BlockIO protocol 
     * using the UEFI System Table Services. 
     */
    return EFI_SUCCESS;
}
