/* VGA hardware: color entries, cursor I/O, screen rendering */
#include "vga.h"
#include "io.h"
#include "display_internal.h"

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_render(void) {
    if (!rendering_enabled) return;

    int start_line = current_line - TERM_HEIGHT + 1 - scroll_offset;
    if (start_line < 0) start_line = 0;

    for (int y = 0; y < TERM_HEIGHT; y++) {
        int buffer_line = start_line + y;
        if (buffer_line >= TERM_HISTORY) buffer_line = TERM_HISTORY - 1;
        if (buffer_line < 0) buffer_line = 0;

        for (int x = 0; x < TERM_WIDTH; x++) {
            char c = term_buffer[buffer_line][x];
            uint8_t color = term_color_buffer[buffer_line][x];
            vga_memory[y * TERM_WIDTH + x] = vga_entry(c, color);
        }
    }
}

void terminal_update_cursor(void) {
    int start_line = current_line - TERM_HEIGHT + 1 - scroll_offset;
    if (start_line < 0) start_line = 0;

    int visible_line = current_line - start_line;
    if (visible_line < 0) visible_line = 0;
    if (visible_line >= TERM_HEIGHT) visible_line = TERM_HEIGHT - 1;

    uint16_t pos = visible_line * VGA_WIDTH + terminal_column;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_show_cursor(void) {
    cursor_visible = 1;
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0x0E);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 0x0F);
    terminal_update_cursor();
}

void terminal_hide_cursor(void) {
    cursor_visible = 0;
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
