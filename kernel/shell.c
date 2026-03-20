#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "commands.h"
#include "fs.h"
#include "aob.h"

#define MAX_COMMAND_LENGTH 256
#define HISTORY_SIZE 10

static char command_buffer[MAX_COMMAND_LENGTH];
static int command_index = 0;

static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;
static int history_index = 0;
static int browsing_history = 0;
static uint8_t user_color = 0;

void shell_print_prompt(void) {
    uint8_t prompt_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(prompt_color);
    terminal_writestring("AO-OS:");
    
    uint8_t path_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(path_color);
    terminal_writestring(fs_getcwd());
    
    terminal_setcolor(prompt_color);
    terminal_writestring("> ");
    terminal_setcolor(user_color);
}

void shell_clear_command(void) {
    memset(command_buffer, 0, MAX_COMMAND_LENGTH);
    command_index = 0;
}

void shell_set_color(uint8_t color) {
    user_color = color;
    terminal_setcolor(color);
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

// Write argc/argv to fixed address 0x00091000 for user programs
// Layout: [uint64_t argc][uint64_t argv[0]..argv[N-1]][strings...]
#define ARGV_BASE 0x00091000
#define ARGV_MAX  16

static void shell_setup_args(const char* full_cmd) {
    volatile uint64_t* base = (volatile uint64_t*)ARGV_BASE;
    // String storage starts after argc + ARGV_MAX pointers (each 8 bytes)
    char* strbuf = (char*)(ARGV_BASE + 8 + ARGV_MAX * 8);
    int strbuf_pos = 0;
    int argc = 0;

    const char* p = full_cmd;
    while (*p && argc < ARGV_MAX) {
        // Skip spaces
        while (*p == ' ') p++;
        if (!*p) break;
        // Find end of token
        const char* tok_start = p;
        while (*p && *p != ' ') p++;
        int tok_len = (int)(p - tok_start);
        // Write pointer into argv slot
        base[1 + argc] = (uint64_t)(uintptr_t)(ARGV_BASE + 8 + ARGV_MAX * 8 + strbuf_pos);
        // Copy string
        for (int i = 0; i < tok_len; i++) strbuf[strbuf_pos++] = tok_start[i];
        strbuf[strbuf_pos++] = '\0';
        argc++;
    }
    base[0] = (uint64_t)argc;
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
    } else if (strncmp(cmd, "diskinfo", cmd_len) == 0 && cmd_len == 8) {
        cmd_diskinfo();
    } else if (strncmp(cmd, "sconsole", cmd_len) == 0 && cmd_len == 8) {
        cmd_sconsole(args);
    } else if (strcmp(cmd, "checkfs") == 0) {
        cmd_checkfs();
    } else if (strcmp(cmd, "install") == 0) {
        cmd_install();
    } else if (strncmp(cmd, "mkdir", cmd_len) == 0 && cmd_len == 5) {
        cmd_mkdir(args);
    } else if (strncmp(cmd, "rmdir", cmd_len) == 0 && cmd_len == 5) {
        cmd_rmdir(args);
    } else if (strncmp(cmd, "cd", cmd_len) == 0 && cmd_len == 2) {
        cmd_cd(args);
    } else if (strncmp(cmd, "pwd", cmd_len) == 0 && cmd_len == 3) {
        cmd_pwd();
    } else if (strncmp(cmd, "ls", cmd_len) == 0 && cmd_len == 2) {
        cmd_ls();
    } else if (strncmp(cmd, "cat", cmd_len) == 0 && cmd_len == 3) {
        cmd_cat(args);
    } else if (strncmp(cmd, "edit", cmd_len) == 0 && cmd_len == 4) {
        cmd_edit(args);
    } else if (strncmp(cmd, "write", cmd_len) == 0 && cmd_len == 5) {
        cmd_write(args);
    } else if (strncmp(cmd, "rm", cmd_len) == 0 && cmd_len == 2) {
        cmd_rm(args);
    } else if (strncmp(cmd, "touch", cmd_len) == 0 && cmd_len == 5) {
        cmd_touch(args);
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
    } else if (strncmp(cmd, "divan", cmd_len) == 0 && cmd_len == 5) {
        cmd_divan();
    } else if (strncmp(cmd, "cp", cmd_len) == 0 && cmd_len == 2) {
        cmd_cp(args);
    } else if (strncmp(cmd, "mv", cmd_len) == 0 && cmd_len == 2) {
        cmd_mv(args);
    } else if (strncmp(cmd, "rename", cmd_len) == 0 && cmd_len == 6) {
        cmd_rename(args);
    } else if (strncmp(cmd, "which", cmd_len) == 0 && cmd_len == 5) {
        cmd_which(args);
    } else if (strncmp(cmd, "tree", cmd_len) == 0 && cmd_len == 4) {
        cmd_tree();
    } else {
        // Check if command ends with .aob (AOB executable)
        if (cmd_len > 4 && strncmp(cmd + cmd_len - 4, ".aob", 4) == 0) {
            // Execute AOB binary
            aob_context_t ctx;
            char filename[256];
            memcpy(filename, cmd, cmd_len);
            filename[cmd_len] = '\0';
            
            if (aob_load(filename, &ctx) == 0) {
                shell_setup_args(cmd);
                aob_execute(&ctx);
                aob_unload(&ctx);
            } else {
                uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                terminal_setcolor(error_color);
                terminal_writestring("Error: Failed to load or execute ");
                terminal_writestring(filename);
                terminal_writestring("\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            }
        } else if (cmd_len > 4 && strncmp(cmd + cmd_len - 4, ".bin", 4) == 0) {
            // Execute raw binary
            char filename[256];
            memcpy(filename, cmd, cmd_len);
            filename[cmd_len] = '\0';
            
            static char bin_buffer[16384];
            uint32_t file_size;
            
            if (fs_read(filename, bin_buffer, &file_size) == 0) {
                if (file_size > 0 && file_size <= 16384) {
                    // Copy binary to fixed address 0x200000 so string literals
                    // resolve correctly (compiled with -Ttext=0x200000)
                    char* exec_addr = (char*)0x00200000;
                    memcpy(exec_addr, bin_buffer, file_size);
                    // Write argc/argv to 0x00091000
                    shell_setup_args(cmd);
                    void (*entry_func)(void) = (void (*)(void))exec_addr;
                    entry_func();
                } else {
                    uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                    terminal_setcolor(error_color);
                    terminal_writestring("Error: Binary size invalid (0 or >16KB)\n");
                    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
                }
            } else {
                uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                terminal_setcolor(error_color);
                terminal_writestring("Error: Failed to load ");
                terminal_writestring(filename);
                terminal_writestring("\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            }
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
    user_color = normal_color;
    
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
        
        if (c == KEY_PAGEUP) {
            terminal_scroll_up();
            continue;
        }
        
        if (c == KEY_PAGEDOWN) {
            terminal_scroll_down();
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
