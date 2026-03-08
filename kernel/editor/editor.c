#include "editor.h"
#include "vga.h"
#include "keyboard.h"
#include "ramfs.h"
#include "string.h"

#define KEY_ESC 27
#define KEY_BACKSPACE 8
#define KEY_ENTER 10

static char buffer[EDITOR_BUFFER_SIZE];
static int cursor = 0;

void editor_open(const char* filename) {
    cursor = 0;
    memset(buffer, 0, EDITOR_BUFFER_SIZE);
    
    uint32_t size;
    if (ramfs_read(filename, buffer, &size) == 0) {
        cursor = size;
    }
    
    terminal_clear();
    
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(title_color);
    terminal_writestring("Editing: ");
    terminal_writestring(filename);
    terminal_writestring("\n");
    
    uint8_t info_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(info_color);
    terminal_writestring("Press ESC to save and exit | PageUp/PageDown to scroll\n");
    terminal_writestring("----------------------------------------\n");
    
    uint8_t text_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(text_color);
    
    for (int i = 0; i < cursor; i++) {
        terminal_putchar(buffer[i]);
    }
    
    int editing = 1;
    while (editing) {
        unsigned char key = keyboard_getchar();
        
        if (key == 0) {
            continue;
        }
        
        if (key == KEY_PAGEUP) {
            terminal_scroll_up();
            continue;
        }
        
        if (key == KEY_PAGEDOWN) {
            terminal_scroll_down();
            continue;
        }
        
        if (key == KEY_ESC) {
            ramfs_create(filename, buffer, cursor);
            
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
            }
        } else if (key == KEY_ENTER) {
            if (cursor < EDITOR_BUFFER_SIZE - 1) {
                buffer[cursor++] = '\n';
                terminal_putchar('\n');
            }
        } else if (key >= 32 && key < 127) {
            if (cursor < EDITOR_BUFFER_SIZE - 1) {
                buffer[cursor++] = key;
                terminal_putchar(key);
            }
        }
    }
    
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
}
