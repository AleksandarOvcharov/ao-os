#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "commands.h"

#define MAX_COMMAND_LENGTH 256

static char command_buffer[MAX_COMMAND_LENGTH];
static int command_index = 0;

void shell_print_prompt(void) {
    uint8_t prompt_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(prompt_color);
    terminal_writestring("AO-OS> ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void shell_clear_command(void) {
    memset(command_buffer, 0, MAX_COMMAND_LENGTH);
    command_index = 0;
}

void shell_execute_command(const char* cmd) {
    if (strlen(cmd) == 0) {
        return;
    }
    
    char* space = (char*)cmd;
    while (*space && *space != ' ') space++;
    
    size_t cmd_len = space - cmd;
    const char* args = (*space) ? space + 1 : "";
    
    if (strncmp(cmd, "help", cmd_len) == 0 && cmd_len == 4) {
        cmd_help();
    } else if (strncmp(cmd, "clear", cmd_len) == 0 && cmd_len == 5) {
        cmd_clear();
    } else if (strncmp(cmd, "echo", cmd_len) == 0 && cmd_len == 4) {
        cmd_echo(args);
    } else if (strncmp(cmd, "about", cmd_len) == 0 && cmd_len == 5) {
        cmd_about();
    } else if (strncmp(cmd, "kernel", cmd_len) == 0 && cmd_len == 6) {
        cmd_kernel(args);
    } else if (strncmp(cmd, "color", cmd_len) == 0 && cmd_len == 5) {
        cmd_color(args);
    } else if (strncmp(cmd, "reboot", cmd_len) == 0 && cmd_len == 6) {
        cmd_reboot();
    } else if (strncmp(cmd, "shutdown", cmd_len) == 0 && cmd_len == 8) {
        cmd_shutdown();
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Unknown command: ");
        terminal_writestring(cmd);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands.\n");
    }
}

void shell_init(void) {
    terminal_clear();
    
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  Welcome to AO OS!\n");
    terminal_writestring("  Aleksandar Ovcharov's Operating System\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("\n");
    terminal_writestring("Type 'help' for available commands.\n");
    terminal_writestring("\n");
    
    shell_clear_command();
}

void shell_run(void) {
    shell_print_prompt();
    
    while (1) {
        char c = keyboard_getchar();
        
        if (c == 0) {
            continue;
        }
        
        if (c == '\n') {
            terminal_putchar('\n');
            command_buffer[command_index] = '\0';
            shell_execute_command(command_buffer);
            shell_clear_command();
            shell_print_prompt();
        } else if (c == '\b') {
            if (command_index > 0) {
                command_index--;
                command_buffer[command_index] = '\0';
                terminal_backspace();
            }
        } else if (command_index < MAX_COMMAND_LENGTH - 1) {
            command_buffer[command_index++] = c;
            terminal_putchar(c);
        }
    }
}
