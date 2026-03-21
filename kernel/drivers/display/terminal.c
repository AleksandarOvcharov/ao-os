/* Terminal buffer management: text output, scrolling, editing */
#include "vga.h"
#include "string.h"
#include "display_internal.h"

/* Shared terminal state (defined here, declared extern in display_internal.h) */
char term_buffer[TERM_HISTORY][TERM_WIDTH];
uint8_t term_color_buffer[TERM_HISTORY][TERM_WIDTH];
int scroll_offset = 0;
int current_line = 0;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* vga_memory;
int cursor_visible = 1;
int rendering_enabled = 1;

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

int terminal_get_row(void) {
    return current_line;
}

int terminal_get_column(void) {
    return terminal_column;
}

void terminal_set_cursor(int row, int col) {
    if (row >= 0 && row < TERM_HISTORY) {
        current_line = row;
    }
    if (col >= 0 && col < TERM_WIDTH) {
        terminal_column = col;
    }
    terminal_update_cursor();
}

void terminal_disable_rendering(void) {
    rendering_enabled = 0;
    terminal_hide_cursor();
}

void terminal_enable_rendering(void) {
    rendering_enabled = 1;
    terminal_render();
    terminal_show_cursor();
}
