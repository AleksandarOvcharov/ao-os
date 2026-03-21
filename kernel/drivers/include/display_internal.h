#ifndef DISPLAY_INTERNAL_H
#define DISPLAY_INTERNAL_H

#include "vga.h"
#include <stdint.h>
#include <stddef.h>

/* Shared terminal state — used by both vga.c and terminal.c */
extern char term_buffer[TERM_HISTORY][TERM_WIDTH];
extern uint8_t term_color_buffer[TERM_HISTORY][TERM_WIDTH];
extern int scroll_offset;
extern int current_line;
extern size_t terminal_column;
extern uint8_t terminal_color;
extern uint16_t* vga_memory;
extern int cursor_visible;
extern int rendering_enabled;

/* VGA hardware functions (vga.c) */
uint16_t vga_entry(unsigned char uc, uint8_t color);

#endif
