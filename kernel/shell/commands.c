#include "commands.h"
#include "vga.h"
#include "system.h"
#include "string.h"
#include "version.h"
#include "memory.h"
#include "timer.h"
#include "cpu.h"
#include "shell.h"
#include "ata.h"
#include "serial.h"
#include "ramfs.h"
#include "editor.h"

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
    terminal_writestring("  sysinfo  - Display system information\n");
    terminal_writestring("  diskinfo - Display disk information\n");
    terminal_writestring("  sconsole - Serial console status (usage: sconsole --status)\n");
    terminal_writestring("  ls       - List files in RAM filesystem\n");
    terminal_writestring("  cat      - Display file contents (usage: cat <filename>)\n");
    terminal_writestring("  edit     - Edit file (usage: edit <filename>)\n");
    terminal_writestring("  write    - Write to file (usage: write <filename> <content>)\n");
    terminal_writestring("  rm       - Remove file (usage: rm <filename>)\n");
    terminal_writestring("  touch    - Create empty file (usage: touch <filename>)\n");
    terminal_writestring("  mem      - Display memory usage information\n");
    terminal_writestring("  uptime   - Display system uptime\n");
    terminal_writestring("  color    - Change text color (usage: color <foreground>)\n");
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
        terminal_writestring("Usage: color <foreground>\n");
        terminal_writestring("Example: color light_green\n");
        return;
    }
    
    char fg_name[32] = {0};
    int i = 0;
    
    while (args[i] && args[i] != ' ' && i < 31) {
        fg_name[i] = args[i];
        i++;
    }
    fg_name[i] = '\0';
    
    enum vga_color fg = parse_color(fg_name);
    
    shell_set_color(vga_entry_color(fg, VGA_COLOR_BLACK));
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

static void print_size(size_t bytes) {
    char buffer[32];
    
    if (bytes >= 1024 * 1024) {
        size_t mb = bytes / (1024 * 1024);
        
        int j = 0;
        size_t temp = mb;
        if (temp == 0) {
            buffer[j++] = '0';
        } else {
            char digits[12];
            int k = 0;
            while (temp > 0) {
                digits[k++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (k > 0) {
                buffer[j++] = digits[--k];
            }
        }
        buffer[j++] = ' ';
        buffer[j++] = 'M';
        buffer[j++] = 'B';
        buffer[j] = '\0';
        terminal_writestring(buffer);
    } else if (bytes >= 1024) {
        size_t kb = bytes / 1024;
        
        int j = 0;
        size_t temp = kb;
        if (temp == 0) {
            buffer[j++] = '0';
        } else {
            char digits[12];
            int k = 0;
            while (temp > 0) {
                digits[k++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (k > 0) {
                buffer[j++] = digits[--k];
            }
        }
        buffer[j++] = ' ';
        buffer[j++] = 'K';
        buffer[j++] = 'B';
        buffer[j] = '\0';
        terminal_writestring(buffer);
    } else {
        int j = 0;
        size_t temp = bytes;
        if (temp == 0) {
            buffer[j++] = '0';
        } else {
            char digits[12];
            int k = 0;
            while (temp > 0) {
                digits[k++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (k > 0) {
                buffer[j++] = digits[--k];
            }
        }
        buffer[j++] = ' ';
        buffer[j++] = 'B';
        buffer[j] = '\0';
        terminal_writestring(buffer);
    }
}

void cmd_mem(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    size_t total = HEAP_SIZE;
    size_t used = memory_get_used();
    size_t free = memory_get_free();
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  Memory Information\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Total Heap:  ");
    terminal_setcolor(value_color);
    print_size(total);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Used:        ");
    terminal_setcolor(value_color);
    print_size(used);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Free:        ");
    terminal_setcolor(value_color);
    print_size(free);
    terminal_writestring("\n");
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
}

void cmd_uptime(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / TIMER_HZ;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    terminal_setcolor(title_color);
    terminal_writestring("System Uptime: ");
    terminal_setcolor(value_color);
    
    char buffer[32];
    
    if (days > 0) {
        int i = 0;
        uint32_t temp = days;
        char digits[12];
        int j = 0;
        while (temp > 0) {
            digits[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            buffer[i++] = digits[--j];
        }
        buffer[i++] = ' ';
        buffer[i++] = 'd';
        buffer[i++] = 'a';
        buffer[i++] = 'y';
        if (days > 1) buffer[i++] = 's';
        buffer[i++] = ' ';
        buffer[i] = '\0';
        terminal_writestring(buffer);
    }
    
    if (hours > 0 || days > 0) {
        int i = 0;
        uint32_t temp = hours;
        if (temp == 0) {
            buffer[i++] = '0';
        } else {
            char digits[12];
            int j = 0;
            while (temp > 0) {
                digits[j++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (j > 0) {
                buffer[i++] = digits[--j];
            }
        }
        buffer[i++] = 'h';
        buffer[i++] = ' ';
        buffer[i] = '\0';
        terminal_writestring(buffer);
    }
    
    if (minutes > 0 || hours > 0 || days > 0) {
        int i = 0;
        uint32_t temp = minutes;
        if (temp == 0) {
            buffer[i++] = '0';
        } else {
            char digits[12];
            int j = 0;
            while (temp > 0) {
                digits[j++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (j > 0) {
                buffer[i++] = digits[--j];
            }
        }
        buffer[i++] = 'm';
        buffer[i++] = ' ';
        buffer[i] = '\0';
        terminal_writestring(buffer);
    }
    
    int i = 0;
    uint32_t temp = seconds;
    if (temp == 0) {
        buffer[i++] = '0';
    } else {
        char digits[12];
        int j = 0;
        while (temp > 0) {
            digits[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            buffer[i++] = digits[--j];
        }
    }
    buffer[i++] = 's';
    buffer[i] = '\0';
    terminal_writestring(buffer);
    
    terminal_setcolor(label_color);
    terminal_writestring("\n");
}

void cmd_sysinfo(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    char cpu_vendor[13];
    cpu_get_vendor(cpu_vendor);
    
    uint32_t ram_kb = cpu_detect_memory();
    
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / TIMER_HZ;
    
    terminal_setcolor(title_color);
    terminal_writestring("AO OS ");
    terminal_setcolor(value_color);
    terminal_writestring(KERNEL_VERSION_STRING);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("CPU:    ");
    terminal_setcolor(value_color);
    terminal_writestring(cpu_vendor);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("RAM:    ");
    terminal_setcolor(value_color);
    
    char buffer[32];
    int i = 0;
    uint32_t temp = ram_kb;
    if (temp == 0) {
        buffer[i++] = '0';
    } else {
        char digits[12];
        int j = 0;
        while (temp > 0) {
            digits[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            buffer[i++] = digits[--j];
        }
    }
    buffer[i++] = ' ';
    buffer[i++] = 'K';
    buffer[i++] = 'B';
    buffer[i] = '\0';
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Uptime: ");
    terminal_setcolor(value_color);
    
    i = 0;
    temp = seconds;
    if (temp == 0) {
        buffer[i++] = '0';
    } else {
        char digits[12];
        int j = 0;
        while (temp > 0) {
            digits[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            buffer[i++] = digits[--j];
        }
    }
    buffer[i++] = 's';
    buffer[i] = '\0';
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
}

void cmd_diskinfo(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  Disk Information\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("ATA Controller: ");
    
    if (ata_is_available()) {
        terminal_setcolor(value_color);
        terminal_writestring("Primary (0x1F0)\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Status:         ");
        terminal_setcolor(value_color);
        terminal_writestring("Available\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Mode:           ");
        terminal_setcolor(value_color);
        terminal_writestring("PIO\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Addressing:     ");
        terminal_setcolor(value_color);
        terminal_writestring("LBA28\n");
    } else {
        terminal_setcolor(error_color);
        terminal_writestring("Not detected\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Status:         ");
        terminal_setcolor(error_color);
        terminal_writestring("Unavailable\n");
    }
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
}

void cmd_sconsole(const char* args) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    if (!args || !*args) {
        terminal_writestring("Usage: sconsole --status\n");
        return;
    }
    
    if (strcmp(args, "--status") == 0) {
        terminal_setcolor(title_color);
        terminal_writestring("========================================\n");
        terminal_writestring("  Serial Console Status\n");
        terminal_writestring("========================================\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Port:           ");
        terminal_setcolor(value_color);
        terminal_writestring("COM1 (0x3F8)\n");
        
        terminal_setcolor(label_color);
        terminal_writestring("Status:         ");
        
        if (serial_is_initialized()) {
            terminal_setcolor(value_color);
            terminal_writestring("Initialized\n");
            
            terminal_setcolor(label_color);
            terminal_writestring("Baud Rate:      ");
            terminal_setcolor(value_color);
            terminal_writestring("115200\n");
            
            terminal_setcolor(label_color);
            terminal_writestring("Data Bits:      ");
            terminal_setcolor(value_color);
            terminal_writestring("8\n");
            
            terminal_setcolor(label_color);
            terminal_writestring("Parity:         ");
            terminal_setcolor(value_color);
            terminal_writestring("None\n");
            
            terminal_setcolor(label_color);
            terminal_writestring("Stop Bits:      ");
            terminal_setcolor(value_color);
            terminal_writestring("1\n");
            
            terminal_setcolor(label_color);
            terminal_writestring("QEMU:           ");
            terminal_setcolor(value_color);
            terminal_writestring("Use -serial stdio\n");
        } else {
            uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_setcolor(error_color);
            terminal_writestring("Not initialized\n");
        }
        
        terminal_setcolor(title_color);
        terminal_writestring("========================================\n");
        
        terminal_setcolor(label_color);
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Unknown option: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(label_color);
        terminal_writestring("Usage: sconsole --status\n");
    }
}

void cmd_ls(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t file_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    int count = ramfs_get_file_count();
    ramfs_file_t* files = ramfs_get_files();
    
    terminal_setcolor(title_color);
    terminal_writestring("Files in RAM filesystem:\n");
    terminal_setcolor(label_color);
    
    if (count == 0) {
        terminal_writestring("  (empty)\n");
        return;
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            terminal_writestring("  ");
            terminal_setcolor(file_color);
            terminal_writestring(files[i].name);
            terminal_setcolor(label_color);
            terminal_writestring(" (");
            
            char size_str[16];
            uint32_t size = files[i].size;
            int j = 0;
            if (size == 0) {
                size_str[j++] = '0';
            } else {
                char digits[12];
                int k = 0;
                while (size > 0) {
                    digits[k++] = '0' + (size % 10);
                    size /= 10;
                }
                while (k > 0) {
                    size_str[j++] = digits[--k];
                }
            }
            size_str[j] = '\0';
            
            terminal_writestring(size_str);
            terminal_writestring(" bytes)\n");
        }
    }
}

void cmd_cat(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: cat <filename>\n");
        return;
    }
    
    char buffer[MAX_FILESIZE];
    uint32_t size;
    
    if (ramfs_read(args, buffer, &size) == 0) {
        for (uint32_t i = 0; i < size; i++) {
            terminal_putchar(buffer[i]);
        }
        terminal_writestring("\n");
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: File not found: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    }
}

void cmd_write(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: write <filename> <content>\n");
        return;
    }
    
    char filename[MAX_FILENAME];
    int i = 0, j = 0;
    
    while (args[i] && args[i] != ' ' && j < MAX_FILENAME - 1) {
        filename[j++] = args[i++];
    }
    filename[j] = '\0';
    
    while (args[i] == ' ') i++;
    
    if (!args[i]) {
        terminal_writestring("Usage: write <filename> <content>\n");
        return;
    }
    
    const char* content = &args[i];
    uint32_t size = strlen(content);
    
    if (size > MAX_FILESIZE) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Content too large (max 512 bytes)\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
        return;
    }
    
    if (ramfs_create(filename, content, size) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("File written: ");
        terminal_writestring(filename);
        terminal_writestring("\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Could not write file (filesystem full?)\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    }
}

void cmd_rm(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: rm <filename>\n");
        return;
    }
    
    if (ramfs_delete(args) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("File removed: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: File not found: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    }
}

void cmd_touch(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: touch <filename>\n");
        return;
    }
    
    if (ramfs_create(args, "", 0) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("File created: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Could not create file (filesystem full or file exists)\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
    }
}

void cmd_edit(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: edit <filename>\n");
        return;
    }
    
    editor_open(args);
}
