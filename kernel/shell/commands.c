#include "commands.h"
#include "vga.h"
#include "system.h"
#include "string.h"
#include "version.h"

void cmd_help(void) {
    uint8_t old_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(old_color);
    
    terminal_writestring("AO OS - Available Commands:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    terminal_writestring("  help     - Display this help message\n");
    terminal_writestring("  clear    - Clear the screen\n");
    terminal_writestring("  echo     - Print text to the screen\n");
    terminal_writestring("  about    - Display OS information\n");
    terminal_writestring("  kernel   - Kernel information (usage: kernel -v or --version)\n");
    terminal_writestring("  color    - Change text color (usage: color <fg> <bg>)\n");
    terminal_writestring("  reboot   - Reboot the system\n");
    terminal_writestring("  shutdown - Shutdown the system\n");
    
    terminal_writestring("\nAvailable colors:\n");
    terminal_writestring("  black, blue, green, cyan, red, magenta, brown,\n");
    terminal_writestring("  light_grey, dark_grey, light_blue, light_green,\n");
    terminal_writestring("  light_cyan, light_red, light_magenta, yellow, white\n");
}

void cmd_clear(void) {
    terminal_clear();
}

void cmd_echo(const char* args) {
    if (args && *args) {
        terminal_writestring(args);
        terminal_writestring("\n");
    }
}

void cmd_about(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t highlight_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  AO OS - Aleksandar Ovcharov's OS\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("Version: ");
    terminal_setcolor(highlight_color);
    terminal_writestring("0.2.0\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("Architecture: ");
    terminal_setcolor(highlight_color);
    terminal_writestring("x86 (32-bit)\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("Author: ");
    terminal_setcolor(highlight_color);
    terminal_writestring("Aleksandar Ovcharov\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("\n");
    terminal_writestring("A simple operating system written in\n");
    terminal_writestring("C and Assembly for educational purposes.\n");
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    
    terminal_setcolor(normal_color);
}

void cmd_reboot(void) {
    uint8_t warning_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal_setcolor(warning_color);
    terminal_writestring("\nRebooting system...\n");
    system_reboot();
}

void cmd_shutdown(void) {
    uint8_t warning_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal_setcolor(warning_color);
    system_shutdown();
}

static enum vga_color parse_color(const char* color_name) {
    if (strcmp(color_name, "black") == 0) return VGA_COLOR_BLACK;
    if (strcmp(color_name, "blue") == 0) return VGA_COLOR_BLUE;
    if (strcmp(color_name, "green") == 0) return VGA_COLOR_GREEN;
    if (strcmp(color_name, "cyan") == 0) return VGA_COLOR_CYAN;
    if (strcmp(color_name, "red") == 0) return VGA_COLOR_RED;
    if (strcmp(color_name, "magenta") == 0) return VGA_COLOR_MAGENTA;
    if (strcmp(color_name, "brown") == 0) return VGA_COLOR_BROWN;
    if (strcmp(color_name, "light_grey") == 0) return VGA_COLOR_LIGHT_GREY;
    if (strcmp(color_name, "dark_grey") == 0) return VGA_COLOR_DARK_GREY;
    if (strcmp(color_name, "light_blue") == 0) return VGA_COLOR_LIGHT_BLUE;
    if (strcmp(color_name, "light_green") == 0) return VGA_COLOR_LIGHT_GREEN;
    if (strcmp(color_name, "light_cyan") == 0) return VGA_COLOR_LIGHT_CYAN;
    if (strcmp(color_name, "light_red") == 0) return VGA_COLOR_LIGHT_RED;
    if (strcmp(color_name, "light_magenta") == 0) return VGA_COLOR_LIGHT_MAGENTA;
    if (strcmp(color_name, "yellow") == 0) return VGA_COLOR_LIGHT_BROWN;
    if (strcmp(color_name, "white") == 0) return VGA_COLOR_WHITE;
    return VGA_COLOR_LIGHT_GREY;
}

void cmd_color(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: color <foreground> <background>\n");
        terminal_writestring("Example: color light_green black\n");
        return;
    }
    
    char fg_name[32] = {0};
    char bg_name[32] = {0};
    int i = 0, j = 0;
    
    while (args[i] && args[i] != ' ' && i < 31) {
        fg_name[j++] = args[i++];
    }
    fg_name[j] = '\0';
    
    while (args[i] == ' ') i++;
    
    j = 0;
    while (args[i] && args[i] != ' ' && j < 31) {
        bg_name[j++] = args[i++];
    }
    bg_name[j] = '\0';
    
    if (bg_name[0] == '\0') {
        strcpy(bg_name, "black");
    }
    
    enum vga_color fg = parse_color(fg_name);
    enum vga_color bg = parse_color(bg_name);
    
    terminal_setcolor(vga_entry_color(fg, bg));
    terminal_writestring("Color changed successfully!\n");
}

void cmd_kernel(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: kernel [--version | -v]\n");
        return;
    }
    
    if (strcmp(args, "--version") == 0 || strcmp(args, "-v") == 0) {
        kernel_print_full_info();
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Unknown option: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Usage: kernel [--version | -v]\n");
    }
}
