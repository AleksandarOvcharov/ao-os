#include "editor.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"

#define KEY_ESC 27
#define KEY_BACKSPACE 8
#define KEY_ENTER 10
#define KEY_CTRL_S 19

static char buffer[EDITOR_BUFFER_SIZE];
static int cursor = 0;
static int current_line = 1;
static int current_col = 1;

static void update_line_col(void) {
    current_line = 1;
    current_col = 1;
    
    for (int i = 0; i < cursor; i++) {
        if (buffer[i] == '\n') {
            current_line++;
            current_col = 1;
        } else {
            current_col++;
        }
    }
}

static char status_message[40] = "";

static void draw_status_bar(const char* filename, int saved) {
    uint16_t* vga_memory = (uint16_t*)0xB8000;
    uint8_t status_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    
    // Clear status bar line (row 24)
    for (int i = 0; i < 80; i++) {
        vga_memory[24 * 80 + i] = ((uint16_t)status_color << 8) | ' ';
    }
    
    // Write filename
    int pos = 1;
    for (int i = 0; filename[i] && pos < 30; i++) {
        vga_memory[24 * 80 + pos] = ((uint16_t)status_color << 8) | filename[i];
        pos++;
    }
    
    // Write status message if present
    if (status_message[0] != '\0') {
        vga_memory[24 * 80 + pos++] = ((uint16_t)status_color << 8) | ' ';
        vga_memory[24 * 80 + pos++] = ((uint16_t)status_color << 8) | '-';
        vga_memory[24 * 80 + pos++] = ((uint16_t)status_color << 8) | ' ';
        for (int i = 0; status_message[i] && pos < 50; i++) {
            vga_memory[24 * 80 + pos] = ((uint16_t)status_color << 8) | status_message[i];
            pos++;
        }
    }
    
    // Build info string
    char info[80];
    char* ptr = info;
    
    *ptr++ = 'L';
    *ptr++ = 'n';
    *ptr++ = ':';
    
    int line = current_line;
    char line_str[8];
    int idx = 0;
    if (line == 0) {
        line_str[idx++] = '0';
    } else {
        char temp[8];
        int t = 0;
        while (line > 0) {
            temp[t++] = '0' + (line % 10);
            line /= 10;
        }
        while (t > 0) {
            line_str[idx++] = temp[--t];
        }
    }
    line_str[idx] = '\0';
    for (int i = 0; line_str[i]; i++) *ptr++ = line_str[i];
    
    *ptr++ = ',';
    *ptr++ = 'C';
    *ptr++ = ':';
    
    int col_num = current_col;
    char col_str[8];
    idx = 0;
    if (col_num == 0) {
        col_str[idx++] = '0';
    } else {
        char temp[8];
        int t = 0;
        while (col_num > 0) {
            temp[t++] = '0' + (col_num % 10);
            col_num /= 10;
        }
        while (t > 0) {
            col_str[idx++] = temp[--t];
        }
    }
    col_str[idx] = '\0';
    for (int i = 0; col_str[i]; i++) *ptr++ = col_str[i];
    
    *ptr++ = ' ';
    *ptr++ = '|';
    *ptr++ = ' ';
    
    int chars = cursor;
    char chars_str[8];
    idx = 0;
    if (chars == 0) {
        chars_str[idx++] = '0';
    } else {
        char temp[8];
        int t = 0;
        while (chars > 0) {
            temp[t++] = '0' + (chars % 10);
            chars /= 10;
        }
        while (t > 0) {
            chars_str[idx++] = temp[--t];
        }
    }
    chars_str[idx] = '\0';
    for (int i = 0; chars_str[i]; i++) *ptr++ = chars_str[i];
    
    *ptr++ = '/';
    
    int max = EDITOR_BUFFER_SIZE - 1;
    char max_str[8];
    idx = 0;
    if (max == 0) {
        max_str[idx++] = '0';
    } else {
        char temp[8];
        int t = 0;
        while (max > 0) {
            temp[t++] = '0' + (max % 10);
            max /= 10;
        }
        while (t > 0) {
            max_str[idx++] = temp[--t];
        }
    }
    max_str[idx] = '\0';
    for (int i = 0; max_str[i]; i++) *ptr++ = max_str[i];
    
    *ptr++ = ' ';
    *ptr++ = '|';
    *ptr++ = ' ';
    *ptr++ = 'E';
    *ptr++ = 'S';
    *ptr++ = 'C';
    *ptr++ = ':';
    *ptr++ = 'E';
    *ptr++ = 'x';
    *ptr++ = 'i';
    *ptr++ = 't';
    *ptr++ = ' ';
    *ptr++ = 'C';
    *ptr++ = 't';
    *ptr++ = 'r';
    *ptr++ = 'l';
    *ptr++ = '+';
    *ptr++ = 'S';
    *ptr++ = ':';
    *ptr++ = 'S';
    *ptr++ = 'a';
    *ptr++ = 'v';
    *ptr++ = 'e';
    
    if (saved) {
        *ptr++ = ' ';
        *ptr++ = '[';
        *ptr++ = 'S';
        *ptr++ = 'A';
        *ptr++ = 'V';
        *ptr++ = 'E';
        *ptr++ = 'D';
        *ptr++ = ']';
    }
    
    *ptr = '\0';
    
    // Write info string at right side of status bar
    int info_len = ptr - info;
    int start_pos = 80 - info_len;
    for (int i = 0; i < info_len; i++) {
        vga_memory[24 * 80 + start_pos + i] = ((uint16_t)status_color << 8) | info[i];
    }
}

void editor_open(const char* filename) {
    cursor = 0;
    memset(buffer, 0, EDITOR_BUFFER_SIZE);
    memset(status_message, 0, sizeof(status_message));
    
    uint32_t size;
    if (fs_read(filename, buffer, &size) == 0) {
        cursor = size;
        const char* msg = "Loaded";
        for (int i = 0; msg[i]; i++) {
            status_message[i] = msg[i];
        }
    } else {
        const char* msg = "New file";
        for (int i = 0; msg[i]; i++) {
            status_message[i] = msg[i];
        }
    }
    
    terminal_clear();
    
    uint8_t text_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(text_color);
    
    update_line_col();
    
    for (int i = 0; i < cursor; i++) {
        terminal_putchar(buffer[i]);
    }
    
    draw_status_bar(filename, 0);
    
    int editing = 1;
    int saved_indicator = 0;
    int last_line = current_line;
    int last_col = current_col;
    int last_cursor = cursor;
    while (editing) {
        unsigned char key = keyboard_getchar();
        
        if (key == 0) {
            continue;
        }
        
        if (key == KEY_PAGEUP) {
            terminal_scroll_up();
            draw_status_bar(filename, saved_indicator);
            continue;
        }
        
        if (key == KEY_PAGEDOWN) {
            terminal_scroll_down();
            draw_status_bar(filename, saved_indicator);
            continue;
        }
        
        if (key == KEY_CTRL_S) {
            fs_create(filename, buffer, cursor);
            saved_indicator = 1;
            const char* msg = "Saved";
            memset(status_message, 0, sizeof(status_message));
            for (int i = 0; msg[i]; i++) {
                status_message[i] = msg[i];
            }
            draw_status_bar(filename, saved_indicator);
            continue;
        }
        
        if (key == KEY_ESC) {
            fs_create(filename, buffer, cursor);
            
            terminal_writestring("\n\n");
            uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            terminal_setcolor(success_color);
            terminal_writestring("File saved: ");
            terminal_writestring(filename);
            terminal_writestring("\n");
            
            editing = 0;
        } else if (key == KEY_BACKSPACE) {
            if (cursor > 0) {
                cursor--;
                buffer[cursor] = '\0';
                terminal_backspace();
                update_line_col();
                saved_indicator = 0;
                memset(status_message, 0, sizeof(status_message));
                if (current_line != last_line || current_col != last_col || cursor != last_cursor) {
                    draw_status_bar(filename, saved_indicator);
                    last_line = current_line;
                    last_col = current_col;
                    last_cursor = cursor;
                }
            }
        } else if (key == KEY_ENTER) {
            if (cursor < EDITOR_BUFFER_SIZE - 1) {
                buffer[cursor++] = '\n';
                terminal_putchar('\n');
                update_line_col();
                saved_indicator = 0;
                memset(status_message, 0, sizeof(status_message));
                if (current_line != last_line || current_col != last_col || cursor != last_cursor) {
                    draw_status_bar(filename, saved_indicator);
                    last_line = current_line;
                    last_col = current_col;
                    last_cursor = cursor;
                }
            }
        } else if (key >= 32 && key < 127) {
            if (cursor < EDITOR_BUFFER_SIZE - 1) {
                buffer[cursor++] = key;
                terminal_putchar(key);
                update_line_col();
                saved_indicator = 0;
                memset(status_message, 0, sizeof(status_message));
                if (current_line != last_line || current_col != last_col || cursor != last_cursor) {
                    draw_status_bar(filename, saved_indicator);
                    last_line = current_line;
                    last_col = current_col;
                    last_cursor = cursor;
                }
            }
        }
    }
    
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
}
