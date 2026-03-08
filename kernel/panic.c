#include "panic.h"
#include "vga.h"
#include "string.h"

static void fill_screen_red(void) {
    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_setcolor(panic_bg);
    
    for (int i = 0; i < 80 * 25; i++) {
        terminal_putchar(' ');
    }
}

static void print_panic_header(void) {
    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    uint8_t panic_title = vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_RED);
    
    terminal_clear();
    fill_screen_red();
    
    terminal_setcolor(panic_title);
    terminal_writestring("\n");
    terminal_writestring("                          *** KERNEL PANIC ***\n");
    terminal_writestring("\n");
    terminal_setcolor(panic_bg);
}

static void print_panic_footer(void) {
    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_setcolor(panic_bg);
    
    terminal_writestring("\n\n");
    terminal_writestring("  The system has been halted to prevent damage.\n");
    terminal_writestring("  Please reboot your computer.\n");
}

void panic(const char* message) {
    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    uint8_t panic_highlight = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_RED);
    
    asm volatile("cli");
    
    print_panic_header();
    
    terminal_setcolor(panic_bg);
    terminal_writestring("  A fatal error has occurred and the system cannot continue.\n\n");
    
    terminal_setcolor(panic_highlight);
    terminal_writestring("  Error: ");
    terminal_setcolor(panic_bg);
    terminal_writestring(message);
    terminal_writestring("\n");
    
    print_panic_footer();
    
    asm volatile("hlt");
    
    while(1) {
        asm volatile("hlt");
    }
}

void panic_assert(const char* file, int line, const char* desc) {
    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    uint8_t panic_highlight = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_RED);
    
    asm volatile("cli");
    
    print_panic_header();
    
    terminal_setcolor(panic_bg);
    terminal_writestring("  Assertion failed!\n\n");
    
    terminal_setcolor(panic_highlight);
    terminal_writestring("  Condition: ");
    terminal_setcolor(panic_bg);
    terminal_writestring(desc);
    terminal_writestring("\n");
    
    terminal_setcolor(panic_highlight);
    terminal_writestring("  File: ");
    terminal_setcolor(panic_bg);
    terminal_writestring(file);
    terminal_writestring("\n");
    
    terminal_setcolor(panic_highlight);
    terminal_writestring("  Line: ");
    terminal_setcolor(panic_bg);
    
    char line_str[12];
    int i = 0;
    int temp = line;
    if (temp == 0) {
        line_str[i++] = '0';
    } else {
        char digits[12];
        int j = 0;
        while (temp > 0) {
            digits[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            line_str[i++] = digits[--j];
        }
    }
    line_str[i] = '\0';
    
    terminal_writestring(line_str);
    terminal_writestring("\n");
    
    print_panic_footer();
    
    asm volatile("hlt");
    
    while(1) {
        asm volatile("hlt");
    }
}
