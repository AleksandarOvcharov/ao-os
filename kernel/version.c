#include "version.h"
#include "vga.h"

void kernel_print_version(void) {
    uint8_t version_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    terminal_setcolor(version_color);
    terminal_writestring("AO OS Kernel ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring(KERNEL_VERSION_STRING);
    terminal_setcolor(normal_color);
    terminal_writestring("\n");
}

void kernel_print_full_info(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  AO OS Kernel Information\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Version:     ");
    terminal_setcolor(value_color);
    terminal_writestring(KERNEL_VERSION_STRING);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Codename:    ");
    terminal_setcolor(value_color);
    terminal_writestring(KERNEL_CODENAME);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Build Date:  ");
    terminal_setcolor(value_color);
    terminal_writestring(KERNEL_BUILD_DATE);
    terminal_writestring("\n");
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
}
