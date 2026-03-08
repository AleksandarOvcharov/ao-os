#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "commands.h"

#define MAX_COMMAND_LENGTH 256
#define HISTORY_SIZE 10

static char command_buffer[MAX_COMMAND_LENGTH];
static int command_index = 0;

static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;
static int history_index = 0;
static int browsing_history = 0;

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

static void shell_add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) {
        return;
    }
    
    if (history_count > 0 && strcmp(history[0], cmd) == 0) {
        return;
    }
    
    for (int i = HISTORY_SIZE - 1; i > 0; i--) {
        strcpy(history[i], history[i - 1]);
    }
    
    strcpy(history[0], cmd);
    
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

static void shell_clear_current_line(void) {
    for (int i = 0; i < command_index; i++) {
        terminal_backspace();
    }
}

static void shell_load_from_history(int index) {
    if (index < 0 || index >= history_count) {
        return;
    }
    
    shell_clear_current_line();
    
    strcpy(command_buffer, history[index]);
    command_index = strlen(command_buffer);
    
    terminal_writestring(command_buffer);
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
    } else if (strncmp(cmd, "sysinfo", cmd_len) == 0 && cmd_len == 7) {
        cmd_sysinfo();
    } else if (strncmp(cmd, "mem", cmd_len) == 0 && cmd_len == 3) {
        cmd_mem();
    } else if (strncmp(cmd, "uptime", cmd_len) == 0 && cmd_len == 6) {
        cmd_uptime();
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
    
    for (int i = 0; i < HISTORY_SIZE; i++) {
        memset(history[i], 0, MAX_COMMAND_LENGTH);
    }
    history_count = 0;
    history_index = 0;
    browsing_history = 0;
    
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
        unsigned char c = keyboard_getchar();
        
        if (c == 0) {
            continue;
        }
        
        if (c == KEY_UP) {
            if (history_count > 0) {
                if (!browsing_history) {
                    browsing_history = 1;
                    history_index = 0;
                } else if (history_index < history_count - 1) {
                    history_index++;
                }
                shell_load_from_history(history_index);
            }
        } else if (c == KEY_DOWN) {
            if (browsing_history) {
                if (history_index > 0) {
                    history_index--;
                    shell_load_from_history(history_index);
                } else {
                    browsing_history = 0;
                    shell_clear_current_line();
                    shell_clear_command();
                }
            }
        } else if (c == '\n') {
            terminal_putchar('\n');
            command_buffer[command_index] = '\0';
            shell_add_to_history(command_buffer);
            shell_execute_command(command_buffer);
            shell_clear_command();
            browsing_history = 0;
            history_index = 0;
            shell_print_prompt();
        } else if (c == '\b') {
            if (command_index > 0) {
                command_index--;
                command_buffer[command_index] = '\0';
                terminal_backspace();
            }
        } else if (c < 0x80 && command_index < MAX_COMMAND_LENGTH - 1) {
            command_buffer[command_index++] = c;
            terminal_putchar(c);
        }
    }
}
