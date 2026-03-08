#include "vga.h"
#include "string.h"
#include "io.h"

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

static char term_buffer[TERM_HISTORY][TERM_WIDTH];
static uint8_t term_color_buffer[TERM_HISTORY][TERM_WIDTH];
static int scroll_offset = 0;
static int current_line = 0;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* vga_memory;
static int cursor_visible = 1;

void terminal_initialize(void) {
    current_line = 0;
    terminal_column = 0;
    scroll_offset = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_memory = (uint16_t*) 0xB8000;
    
    for (int i = 0; i < TERM_HISTORY; i++) {
        for (int j = 0; j < TERM_WIDTH; j++) {
            term_buffer[i][j] = ' ';
            term_color_buffer[i][j] = terminal_color;
        }
    }
    
    terminal_render();
    terminal_show_cursor();
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_render(void) {
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

void terminal_newline(void) {
    terminal_column = 0;
    current_line++;
    
    if (current_line >= TERM_HISTORY) {
        for (int i = 0; i < TERM_HISTORY - 1; i++) {
            for (int j = 0; j < TERM_WIDTH; j++) {
                term_buffer[i][j] = term_buffer[i + 1][j];
                term_color_buffer[i][j] = term_color_buffer[i + 1][j];
            }
        }
        current_line = TERM_HISTORY - 1;
        for (int j = 0; j < TERM_WIDTH; j++) {
            term_buffer[current_line][j] = ' ';
            term_color_buffer[current_line][j] = terminal_color;
        }
    }
    
    scroll_offset = 0;
    terminal_render();
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_newline();
        if (cursor_visible) terminal_update_cursor();
        return;
    }
    if (c == '\t') {
        terminal_column = (terminal_column + 4) & ~(4 - 1);
        if (terminal_column >= VGA_WIDTH) {
            terminal_newline();
        }
        if (cursor_visible) terminal_update_cursor();
        return;
    }
    
    term_buffer[current_line][terminal_column] = c;
    term_color_buffer[current_line][terminal_column] = terminal_color;
    
    if (++terminal_column == VGA_WIDTH) {
        terminal_newline();
    } else {
        scroll_offset = 0;
        terminal_render();
    }
    
    if (cursor_visible) terminal_update_cursor();
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

void terminal_clear(void) {
    current_line = 0;
    terminal_column = 0;
    scroll_offset = 0;
    
    for (int i = 0; i < TERM_HISTORY; i++) {
        for (int j = 0; j < TERM_WIDTH; j++) {
            term_buffer[i][j] = ' ';
            term_color_buffer[i][j] = terminal_color;
        }
    }
    
    terminal_render();
    if (cursor_visible) terminal_update_cursor();
}

void terminal_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
        term_buffer[current_line][terminal_column] = ' ';
        term_color_buffer[current_line][terminal_column] = terminal_color;
        scroll_offset = 0;
        terminal_render();
        if (cursor_visible) terminal_update_cursor();
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

void terminal_scroll_up(void) {
    scroll_offset += 1;
    if (scroll_offset > current_line) {
        scroll_offset = current_line;
    }
    terminal_render();
    terminal_update_cursor();
}

void terminal_scroll_down(void) {
    scroll_offset -= 1;
    if (scroll_offset < 0) {
        scroll_offset = 0;
    }
    terminal_render();
    terminal_update_cursor();
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
