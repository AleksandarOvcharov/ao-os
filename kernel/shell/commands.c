#include "commands.h"
#include "vga.h"
#include "system.h"
#include "string.h"
#include "version.h"
#include "memory.h"
#include "pmm.h"
#include "timer.h"
#include "cpu.h"
#include "shell.h"
#include "ata.h"
#include "serial.h"
#include "fs.h"
#include "editor.h"
#include "installer.h"
#include "keyboard.h"
#include "aob.h"
#include "rtc.h"

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
    terminal_writestring("  checkfs  - Display current filesystem type\n");
    terminal_writestring("  install  - Install AO OS to disk\n");
    terminal_writestring("  ls       - List files in filesystem\n");
    terminal_writestring("  cat      - Display file contents (usage: cat <filename>)\n");
    terminal_writestring("  edit     - Edit file (usage: edit <filename>)\n");
    terminal_writestring("  write    - Write to file (usage: write <filename> <content>)\n");
    terminal_writestring("  rm       - Remove file (usage: rm <filename>)\n");
    terminal_writestring("  touch    - Create empty file (usage: touch <filename>)\n");
    terminal_writestring("  mkdir    - Create directory (usage: mkdir <dirname>)\n");
    terminal_writestring("  rmdir    - Remove directory (usage: rmdir <dirname>)\n");
    terminal_writestring("  cd       - Change directory (usage: cd <dirname>)\n");
    terminal_writestring("  pwd      - Print working directory\n");
    terminal_writestring("  cp       - Copy file (usage: cp <src> <dest>)\n");
    terminal_writestring("  mv       - Move file (usage: mv <src> <dest>)\n");
    terminal_writestring("  rename   - Rename file (usage: rename <old> <new>)\n");
    terminal_writestring("  which    - Find command or file (usage: which <name>)\n");
    terminal_writestring("  tree     - Show directory tree\n");
    terminal_writestring("  hexdump  - Hex dump of file (usage: hexdump <filename>)\n");
    terminal_writestring("  wc       - Word/line/char count (usage: wc <filename>)\n");
    terminal_writestring("  head     - Show first N lines (usage: head <filename> [N])\n");
    terminal_writestring("  mem      - Display memory usage information\n");
    terminal_writestring("  uptime   - Display system uptime\n");
    terminal_writestring("  date     - Display current date and time\n");
    terminal_writestring("  sleep    - Wait N seconds (usage: sleep <seconds>)\n");
    terminal_writestring("  history  - Show command history\n");
    terminal_writestring("  neofetch - Display system info with ASCII art\n");
    terminal_writestring("  calc     - Calculator (usage: calc 10 + 3)\n");
    terminal_writestring("  color    - Change text color (usage: color <foreground>)\n");
    terminal_writestring("  reboot   - Restart the computer\n");
    terminal_writestring("  shutdown - Shutdown the computer\n");
    terminal_writestring("\n");
    terminal_writestring("Executables:\n");
    terminal_writestring("  *.aob    - Run AOB executable (e.g., hello.aob)\n");
    terminal_writestring("  *.bin    - Run raw binary (e.g., hello.bin)\n");
    
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
    terminal_writestring(KERNEL_VERSION_STRING "\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("Architecture: ");
    terminal_setcolor(highlight_color);
#if defined(__x86_64__) || defined(__LP64__)
    terminal_writestring("x86_64 (64-bit)\n");
#else
    terminal_writestring("x86 (32-bit)\n");
#endif
    
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
    uint8_t dim_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  Memory Information\n");
    terminal_writestring("========================================\n");

    /* Physical memory (from E820 + PMM) */
    terminal_setcolor(title_color);
    terminal_writestring("Physical Memory:\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Total RAM:   ");
    terminal_setcolor(value_color);
    print_size(pmm_get_total_memory());
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Free Pages:  ");
    terminal_setcolor(value_color);
    print_size(pmm_get_free_pages() * 4096);
    terminal_setcolor(dim_color);
    terminal_writestring("  (4 KB pages)\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Used Pages:  ");
    terminal_setcolor(value_color);
    print_size((pmm_get_total_pages() - pmm_get_free_pages()) * 4096);
    terminal_writestring("\n");

    /* Kernel heap */
    terminal_setcolor(title_color);
    terminal_writestring("Kernel Heap:\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Total Heap:  ");
    terminal_setcolor(value_color);
    print_size(HEAP_SIZE);
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Used:        ");
    terminal_setcolor(value_color);
    print_size(memory_get_used());
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("  Free:        ");
    terminal_setcolor(value_color);
    print_size(memory_get_free());
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

    const cpu_info_t* cpu = cpu_get_info();

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
    /* Show brand string (trimmed of leading spaces) */
    const char* brand = cpu->brand;
    while (*brand == ' ') brand++;
    if (*brand) {
        terminal_writestring(brand);
    } else {
        terminal_writestring(cpu->vendor);
    }
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("Arch:   ");
    terminal_setcolor(value_color);
    terminal_writestring("x86_64 (64-bit)");
    if (cpu->ext_features & CPU_EXT_NX)
        terminal_writestring(", NX");
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("Feat:   ");
    terminal_setcolor(value_color);
    if (cpu->features_edx & CPU_FEAT_FPU) terminal_writestring("FPU ");
    if (cpu->features_edx & CPU_FEAT_SSE) terminal_writestring("SSE ");
    if (cpu->features_edx & CPU_FEAT_SSE2) terminal_writestring("SSE2 ");
    if (cpu->features_ecx & CPU_FEAT_SSE3) terminal_writestring("SSE3 ");
    if (cpu->features_ecx & CPU_FEAT_SSE41) terminal_writestring("SSE4.1 ");
    if (cpu->features_ecx & CPU_FEAT_SSE42) terminal_writestring("SSE4.2 ");
    if (cpu->features_ecx & CPU_FEAT_AVX) terminal_writestring("AVX ");
    if (cpu->features_edx & CPU_FEAT_APIC) terminal_writestring("APIC ");
    terminal_writestring("\n");

    terminal_setcolor(label_color);
    terminal_writestring("RAM:    ");
    terminal_setcolor(value_color);
    print_size(pmm_get_total_memory());
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Uptime: ");
    terminal_setcolor(value_color);

    char buffer[32];
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
    terminal_writestring("\n");

    // Disk space
    uint32_t total_kb, used_kb, free_kb;
    fs_get_disk_info(&total_kb, &used_kb, &free_kb);

    if (total_kb > 0) {
        uint8_t warn_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

        // Helper lambda-like macro to print a uint32 KB value
        #define PRINT_KB(val) do { \
            uint32_t _v = (val); \
            char _d[12]; int _j = 0; \
            if (_v == 0) { terminal_putchar('0'); } \
            else { while (_v > 0) { _d[_j++] = '0' + (_v % 10); _v /= 10; } \
                   while (_j > 0) terminal_putchar(_d[--_j]); } \
            terminal_writestring(" KB"); \
        } while(0)

        terminal_setcolor(label_color);
        terminal_writestring("Disk:   ");
        terminal_setcolor(value_color);
        PRINT_KB(total_kb);
        terminal_writestring(" total  ");

        terminal_setcolor(value_color);
        PRINT_KB(used_kb);
        terminal_writestring(" used  ");

        // Free in a different color if low (<10%)
        if (free_kb * 10 < total_kb) {
            terminal_setcolor(warn_color);
        } else {
            terminal_setcolor(value_color);
        }
        PRINT_KB(free_kb);
        terminal_writestring(" free\n");

        // Usage bar [##########..........] 20 chars
        terminal_setcolor(label_color);
        terminal_writestring("        [");
        int filled = (total_kb > 0) ? (int)((used_kb * 20) / total_kb) : 0;
        for (int b = 0; b < 20; b++) {
            if (b < filled) {
                terminal_setcolor(value_color);
                terminal_putchar('#');
            } else {
                terminal_setcolor(label_color);
                terminal_putchar('.');
            }
        }
        terminal_setcolor(label_color);
        terminal_writestring("] ");

        // Percentage
        uint32_t pct = (total_kb > 0) ? ((used_kb * 100) / total_kb) : 0;
        char pct_buf[8]; int pi = 0;
        if (pct == 0) { pct_buf[pi++] = '0'; }
        else { char pd[4]; int pj = 0;
               uint32_t pv = pct;
               while (pv > 0) { pd[pj++] = '0' + (pv % 10); pv /= 10; }
               while (pj > 0) pct_buf[pi++] = pd[--pj]; }
        pct_buf[pi++] = '%'; pct_buf[pi] = '\0';
        terminal_setcolor(value_color);
        terminal_writestring(pct_buf);
        terminal_writestring("\n");

        #undef PRINT_KB
    } else {
        terminal_setcolor(label_color);
        terminal_writestring("Disk:   N/A (ramfs)\n");
    }

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
    fs_file_info_t* files;
    int count;
    
    if (fs_list(&files, &count) != 0) {
        terminal_writestring("Failed to list files\n");
        return;
    }
    
    if (count == 0) {
        terminal_writestring("No files found\n");
        return;
    }
    
    uint8_t dir_color = vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    uint8_t file_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t size_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    for (int i = 0; i < count && i < 224; i++) {
        if (!files[i].used) continue;
        
        if (files[i].is_directory) {
            terminal_setcolor(dir_color);
            terminal_writestring("[DIR] ");
            terminal_writestring(files[i].name);
            terminal_writestring("\n");
        } else {
            terminal_setcolor(file_color);
            terminal_writestring(files[i].name);
            
            terminal_setcolor(size_color);
            terminal_writestring(" (");
            
            char size_str[16];
            uint32_t size = files[i].size;
            int idx = 0;
            
            if (size == 0) {
                size_str[idx++] = '0';
            } else {
                char temp[16];
                int t = 0;
                while (size > 0) {
                    temp[t++] = '0' + (size % 10);
                    size /= 10;
                }
                while (t > 0) {
                    size_str[idx++] = temp[--t];
                }
            }
            size_str[idx] = '\0';
            
            terminal_writestring(size_str);
            terminal_writestring(" bytes)\n");
        }
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void cmd_cat(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: cat <filename>\n");
        return;
    }
    
    char buffer[FS_MAX_FILESIZE];
    uint32_t size;
    
    if (fs_read(args, buffer, &size) == 0) {
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
    
    char filename[FS_MAX_FILENAME];
    int i = 0, j = 0;
    
    while (args[i] && args[i] != ' ' && j < FS_MAX_FILENAME - 1) {
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
    
    if (size > FS_MAX_FILESIZE) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Content too large (max 512 bytes)\n");
        uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(label_color);
        return;
    }
    
    if (fs_create(filename, content, size) == 0) {
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
    
    if (fs_delete(args) == 0) {
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
    
    if (fs_create(args, "", 0) == 0) {
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
        terminal_writestring("Error: Cannot create file (must be in root /)\n");
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

void cmd_checkfs(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    terminal_writestring("  Filesystem Information\n");
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Current filesystem: ");
    terminal_setcolor(value_color);
    terminal_writestring(fs_get_type());
    terminal_writestring("\n");
    
    terminal_setcolor(label_color);
    terminal_writestring("Status: ");
    terminal_setcolor(value_color);
    
    const char* fs_type = fs_get_type();
    if (strcmp(fs_type, "FAT12") == 0) {
        terminal_writestring("Persistent storage (disk-backed)\n");
    } else {
        terminal_writestring("Volatile storage (RAM-backed)\n");
    }
    
    terminal_setcolor(title_color);
    terminal_writestring("========================================\n");
    
    terminal_setcolor(label_color);
}

void cmd_install(void) {
    uint8_t warning_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    terminal_setcolor(warning_color);
    terminal_writestring("\n");
    terminal_writestring("WARNING: This will erase all data on the disk!\n");
    terminal_writestring("\n");
    terminal_setcolor(normal_color);
    terminal_writestring("Are you sure you want to continue? (y/n): ");
    
    unsigned char key = 0;
    while (1) {
        key = keyboard_getchar();
        if (key == 'y' || key == 'Y') {
            terminal_writestring("y\n\n");
            break;
        } else if (key == 'n' || key == 'N') {
            terminal_writestring("n\n");
            terminal_writestring("Installation cancelled.\n");
            return;
        }
    }
    
    int result = installer_run();
    
    // Reinitialize filesystem to reload from freshly formatted disk
    if (result == 0) {
        terminal_writestring("\nReinitializing filesystem...\n");
        fs_init();
        terminal_writestring("Filesystem reloaded.\n");
    }
}

void cmd_mkdir(const char* args) {
    if (!args || args[0] == '\0') {
        terminal_writestring("Usage: mkdir <dirname>\n");
        return;
    }
    
    if (fs_mkdir(args) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("Directory created: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Could not create directory\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    }
}

void cmd_rmdir(const char* args) {
    if (!args || args[0] == '\0') {
        terminal_writestring("Usage: rmdir <dirname>\n");
        return;
    }
    
    if (fs_rmdir(args) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("Directory removed: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Error: Directory not found or not empty\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    }
}

void cmd_cd(const char* args) {
    if (!args || args[0] == '\0') {
        terminal_writestring("Usage: cd <dirname>\n");
        return;
    }
    
    if (fs_chdir(args) == 0) {
        uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_setcolor(success_color);
        terminal_writestring("Changed directory to: ");
        terminal_writestring(fs_getcwd());
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("Failed to change directory\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    }
}

void cmd_pwd(void) {
    uint8_t path_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(path_color);
    terminal_writestring(fs_getcwd());
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void cmd_divan(void) {
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
    terminal_writestring("Ne\n");
}

// Helper: split "src dest" into two tokens
static int split_two_args(const char* args, char* a, int alen, char* b, int blen) {
    int i = 0, j = 0;
    while (args[i] && args[i] != ' ' && j < alen - 1) a[j++] = args[i++];
    a[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] && j < blen - 1) b[j++] = args[i++];
    b[j] = '\0';
    return (a[0] && b[0]) ? 0 : -1;
}

void cmd_cp(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: cp <src> <dest>\n");
        return;
    }

    char src[FS_MAX_FILENAME], dest[FS_MAX_FILENAME];
    if (split_two_args(args, src, FS_MAX_FILENAME, dest, FS_MAX_FILENAME) != 0) {
        terminal_writestring("Usage: cp <src> <dest>\n");
        return;
    }

    static char buf[FS_MAX_FILESIZE];
    uint32_t size;
    if (fs_read(src, buf, &size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: Source not found: ");
        terminal_writestring(src);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    if (fs_create(dest, buf, size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: Could not create: ");
        terminal_writestring(dest);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring(src);
    terminal_writestring(" -> ");
    terminal_writestring(dest);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void cmd_mv(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: mv <src> <dest>\n");
        return;
    }

    char src[FS_MAX_FILENAME], dest[FS_MAX_FILENAME];
    if (split_two_args(args, src, FS_MAX_FILENAME, dest, FS_MAX_FILENAME) != 0) {
        terminal_writestring("Usage: mv <src> <dest>\n");
        return;
    }

    static char buf[FS_MAX_FILESIZE];
    uint32_t size;
    if (fs_read(src, buf, &size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: Source not found: ");
        terminal_writestring(src);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    if (fs_create(dest, buf, size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: Could not create: ");
        terminal_writestring(dest);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    fs_delete(src);
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring(src);
    terminal_writestring(" => ");
    terminal_writestring(dest);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void cmd_rename(const char* args) {
    // rename is just mv
    cmd_mv(args);
}

void cmd_which(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: which <name>\n");
        return;
    }

    // Built-in commands list
    static const char* builtins[] = {
        "help","clear","echo","about","kernel","sysinfo","diskinfo",
        "sconsole","checkfs","install","ls","cat","edit","write","rm",
        "touch","mkdir","rmdir","cd","pwd","mem","uptime","color",
        "reboot","shutdown","cp","mv","rename","which","tree",
        "date","hexdump","wc","head","sleep","history","neofetch","calc",
        "divan", 0
    };

    for (int i = 0; builtins[i]; i++) {
        if (strcmp(builtins[i], args) == 0) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            terminal_writestring(args);
            terminal_writestring(": shell built-in\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            return;
        }
    }

    // Search filesystem for a matching .bin or .aob
    fs_file_info_t* files;
    int count;
    if (fs_list(&files, &count) == 0) {
        for (int i = 0; i < count; i++) {
            if (!files[i].used || files[i].is_directory) continue;
            if (strcmp(files[i].name, args) == 0) {
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
                terminal_writestring(files[i].name);
                terminal_writestring(": executable on disk\n");
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
                return;
            }
        }
    }

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring(args);
    terminal_writestring(": not found\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

// Helper to print uint32 without libc
static void print_uint32(uint32_t n) {
    char tmp[12];
    int t = 0;
    if (n == 0) { terminal_putchar('0'); return; }
    while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
    while (t > 0) terminal_putchar(tmp[--t]);
}

static void tree_print_dir(const char* path, int depth) {
    fs_file_info_t* files;
    int count;

    // Enter directory
    if (depth > 0) {
        if (fs_chdir(path) != 0) return;
    }

    if (fs_list(&files, &count) != 0) return;

    uint8_t dir_color  = vga_entry_color(VGA_COLOR_LIGHT_BLUE,  VGA_COLOR_BLACK);
    uint8_t file_color = vga_entry_color(VGA_COLOR_LIGHT_GREY,  VGA_COLOR_BLACK);
    uint8_t size_color = vga_entry_color(VGA_COLOR_DARK_GREY,   VGA_COLOR_BLACK);

    for (int i = 0; i < count; i++) {
        if (!files[i].used) continue;

        // Indent
        for (int d = 0; d < depth; d++) terminal_writestring("  ");
        terminal_writestring("|-- ");

        if (files[i].is_directory) {
            terminal_setcolor(dir_color);
            terminal_writestring("[");
            terminal_writestring(files[i].name);
            terminal_writestring("]\n");
            terminal_setcolor(file_color);
            // Recurse (max depth 4 to avoid infinite loops)
            if (depth < 4) {
                tree_print_dir(files[i].name, depth + 1);
                // Go back up
                fs_chdir("..");
            }
        } else {
            terminal_setcolor(file_color);
            terminal_writestring(files[i].name);
            terminal_setcolor(size_color);
            terminal_writestring(" (");
            print_uint32(files[i].size);
            terminal_writestring("B)\n");
        }
    }
    terminal_setcolor(file_color);
}

void cmd_tree(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(title_color);
    terminal_writestring(fs_getcwd());
    terminal_writestring("\n");
    tree_print_dir(".", 0);
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

// ═══════════════════════════════════════════════════════════════════
// date / time — read from CMOS RTC
// ═══════════════════════════════════════════════════════════════════

static void print_2digit(uint8_t val) {
    terminal_putchar('0' + val / 10);
    terminal_putchar('0' + val % 10);
}

void cmd_date(void) {
    rtc_time_t t;
    rtc_read(&t);

    static const char* wday_names[] = {
        "???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char* month_names[] = {
        "???","Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };

    uint8_t val = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t lbl = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    terminal_setcolor(val);
    if (t.weekday >= 1 && t.weekday <= 7)
        terminal_writestring(wday_names[t.weekday]);
    else
        terminal_writestring("???");

    terminal_setcolor(lbl);
    terminal_writestring(" ");

    terminal_setcolor(val);
    if (t.month >= 1 && t.month <= 12)
        terminal_writestring(month_names[t.month]);
    else
        terminal_writestring("???");

    terminal_writestring(" ");
    print_2digit(t.day);
    terminal_writestring(" ");
    print_uint32(t.year);
    terminal_writestring(" ");
    print_2digit(t.hour);
    terminal_putchar(':');
    print_2digit(t.minute);
    terminal_putchar(':');
    print_2digit(t.second);

    terminal_setcolor(lbl);
    terminal_writestring(" UTC\n");
}

// ═══════════════════════════════════════════════════════════════════
// hexdump — hex view of a file
// ═══════════════════════════════════════════════════════════════════

static void print_hex_byte(uint8_t b) {
    static const char hex[] = "0123456789ABCDEF";
    terminal_putchar(hex[b >> 4]);
    terminal_putchar(hex[b & 0x0F]);
}

static void print_hex32(uint32_t v) {
    for (int i = 28; i >= 0; i -= 4) {
        static const char hex[] = "0123456789ABCDEF";
        terminal_putchar(hex[(v >> i) & 0xF]);
    }
}

void cmd_hexdump(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: hexdump <filename>\n");
        return;
    }

    static char buf[FS_MAX_FILESIZE];
    uint32_t size;
    if (fs_read(args, buf, &size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: File not found: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }

    uint8_t off_color  = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t hex_color  = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t asc_color  = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t norm_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (uint32_t off = 0; off < size; off += 16) {
        // Offset
        terminal_setcolor(off_color);
        print_hex32(off);
        terminal_writestring("  ");

        // Hex bytes
        terminal_setcolor(hex_color);
        for (int i = 0; i < 16; i++) {
            if (off + i < size) {
                print_hex_byte((uint8_t)buf[off + i]);
            } else {
                terminal_writestring("  ");
            }
            terminal_putchar(' ');
            if (i == 7) terminal_putchar(' ');
        }

        terminal_writestring(" ");

        // ASCII
        terminal_setcolor(asc_color);
        terminal_putchar('|');
        for (int i = 0; i < 16; i++) {
            if (off + i < size) {
                char c = buf[off + i];
                terminal_putchar((c >= 32 && c < 127) ? c : '.');
            }
        }
        terminal_putchar('|');
        terminal_writestring("\n");
    }

    terminal_setcolor(norm_color);
    print_uint32(size);
    terminal_writestring(" bytes\n");
}

// ═══════════════════════════════════════════════════════════════════
// wc — word / line / character count
// ═══════════════════════════════════════════════════════════════════

void cmd_wc(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: wc <filename>\n");
        return;
    }

    static char buf[FS_MAX_FILESIZE];
    uint32_t size;
    if (fs_read(args, buf, &size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: File not found: ");
        terminal_writestring(args);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }

    uint32_t lines = 0, words = 0, chars = size;
    int in_word = 0;

    for (uint32_t i = 0; i < size; i++) {
        if (buf[i] == '\n') lines++;
        if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' || buf[i] == '\r') {
            in_word = 0;
        } else {
            if (!in_word) { words++; in_word = 1; }
        }
    }
    if (size > 0 && buf[size - 1] != '\n') lines++;  // count last line

    uint8_t val = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t lbl = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    terminal_setcolor(val);
    print_uint32(lines);
    terminal_setcolor(lbl);
    terminal_writestring(" lines  ");
    terminal_setcolor(val);
    print_uint32(words);
    terminal_setcolor(lbl);
    terminal_writestring(" words  ");
    terminal_setcolor(val);
    print_uint32(chars);
    terminal_setcolor(lbl);
    terminal_writestring(" chars  ");
    terminal_writestring(args);
    terminal_writestring("\n");
}

// ═══════════════════════════════════════════════════════════════════
// head — show first N lines (default 10)
// ═══════════════════════════════════════════════════════════════════

void cmd_head(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: head <filename> [lines]\n");
        return;
    }

    // Parse: filename [count]
    char filename[256];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 255) { filename[i] = args[i]; i++; }
    filename[i] = '\0';

    int max_lines = 10;
    if (args[i] == ' ') {
        i++;
        max_lines = 0;
        while (args[i] >= '0' && args[i] <= '9') {
            max_lines = max_lines * 10 + (args[i] - '0');
            i++;
        }
        if (max_lines == 0) max_lines = 10;
    }

    static char buf[FS_MAX_FILESIZE];
    uint32_t size;
    if (fs_read(filename, buf, &size) != 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Error: File not found: ");
        terminal_writestring(filename);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }

    int line_count = 0;
    for (uint32_t j = 0; j < size && line_count < max_lines; j++) {
        terminal_putchar(buf[j]);
        if (buf[j] == '\n') line_count++;
    }
    if (size > 0 && buf[size - 1] != '\n') terminal_putchar('\n');
}

// ═══════════════════════════════════════════════════════════════════
// sleep — wait N seconds
// ═══════════════════════════════════════════════════════════════════

void cmd_sleep(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: sleep <seconds>\n");
        return;
    }

    int secs = 0;
    int i = 0;
    while (args[i] >= '0' && args[i] <= '9') {
        secs = secs * 10 + (args[i] - '0');
        i++;
    }

    if (secs <= 0 || secs > 3600) {
        terminal_writestring("Usage: sleep <seconds> (1-3600)\n");
        return;
    }

    timer_wait((uint32_t)secs * TIMER_HZ);
}

// ═══════════════════════════════════════════════════════════════════
// history — show command history
// ═══════════════════════════════════════════════════════════════════

extern char history[10][256];
extern int history_count;

void cmd_history(void) {
    if (history_count == 0) {
        terminal_writestring("No history.\n");
        return;
    }

    uint8_t idx_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t cmd_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (int i = history_count - 1; i >= 0; i--) {
        terminal_setcolor(idx_color);
        print_uint32((uint32_t)(history_count - i));
        terminal_writestring("  ");
        terminal_setcolor(cmd_color);
        terminal_writestring(history[i]);
        terminal_writestring("\n");
    }
}

// ═══════════════════════════════════════════════════════════════════
// neofetch — ASCII art system info
// ═══════════════════════════════════════════════════════════════════

void cmd_neofetch(void) {
    uint8_t art_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t lbl_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t val_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t sep_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    char cpu_vendor[13];
    cpu_get_vendor(cpu_vendor);

    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / TIMER_HZ;
    uint32_t mins  = secs / 60;
    uint32_t hours = mins / 60;
    secs %= 60; mins %= 60;

    rtc_time_t t;
    rtc_read(&t);

    // Line 1: logo + user@ao-os
    terminal_setcolor(art_color);
    terminal_writestring("     _    ___     ___  ____    ");
    terminal_setcolor(val_color);
    terminal_writestring("root");
    terminal_setcolor(sep_color);
    terminal_writestring("@");
    terminal_setcolor(val_color);
    terminal_writestring("ao-os\n");

    // Line 2
    terminal_setcolor(art_color);
    terminal_writestring("    / \\  / _ \\   / _ \\/ ___|   ");
    terminal_setcolor(sep_color);
    terminal_writestring("---------------------\n");

    // Line 3: OS
    terminal_setcolor(art_color);
    terminal_writestring("   / _ \\| | | | | | | \\___ \\   ");
    terminal_setcolor(lbl_color);
    terminal_writestring("OS: ");
    terminal_setcolor(val_color);
    terminal_writestring("AO OS " KERNEL_VERSION_STRING " " KERNEL_CODENAME "\n");

    // Line 4: Arch
    terminal_setcolor(art_color);
    terminal_writestring("  / ___ \\ |_| | | |_| |___) |  ");
    terminal_setcolor(lbl_color);
    terminal_writestring("Arch: ");
    terminal_setcolor(val_color);
#if defined(__x86_64__) || defined(__LP64__)
    terminal_writestring("x86_64\n");
#else
    terminal_writestring("x86\n");
#endif

    // Line 5: CPU
    terminal_setcolor(art_color);
    terminal_writestring(" /_/   \\_\\___/   \\___/|____/   ");
    terminal_setcolor(lbl_color);
    terminal_writestring("CPU: ");
    terminal_setcolor(val_color);
    {
        const cpu_info_t* cpuinfo = cpu_get_info();
        const char* b = cpuinfo->brand;
        while (*b == ' ') b++;
        if (*b) terminal_writestring(b);
        else terminal_writestring(cpu_vendor);
    }
    terminal_writestring("\n");

    // Line 6: Memory
    terminal_setcolor(art_color);
    terminal_writestring("                               ");
    terminal_setcolor(lbl_color);
    terminal_writestring("Mem: ");
    terminal_setcolor(val_color);
    print_size(memory_get_used());
    terminal_writestring(" / ");
    print_size(pmm_get_total_memory());
    terminal_writestring("\n");

    // Line 7: Uptime
    terminal_setcolor(art_color);
    terminal_writestring("                               ");
    terminal_setcolor(lbl_color);
    terminal_writestring("Uptime: ");
    terminal_setcolor(val_color);
    if (hours > 0) { print_uint32(hours); terminal_writestring("h "); }
    print_uint32(mins); terminal_writestring("m ");
    print_uint32(secs); terminal_writestring("s\n");

    // Line 8: Shell
    terminal_writestring("                               ");
    terminal_setcolor(lbl_color);
    terminal_writestring("Shell: ");
    terminal_setcolor(val_color);
    terminal_writestring("aosh\n");

    // Line 9: Time
    terminal_writestring("                               ");
    terminal_setcolor(lbl_color);
    terminal_writestring("Time: ");
    terminal_setcolor(val_color);
    print_2digit(t.hour);
    terminal_putchar(':');
    print_2digit(t.minute);
    terminal_putchar(':');
    print_2digit(t.second);
    terminal_writestring(" UTC\n");

    // Color palette bar
    terminal_writestring("                               ");
    for (int c = 0; c < 8; c++) {
        terminal_setcolor(vga_entry_color((enum vga_color)c, (enum vga_color)c));
        terminal_writestring("  ");
    }
    terminal_writestring("\n                               ");
    for (int c = 8; c < 16; c++) {
        terminal_setcolor(vga_entry_color((enum vga_color)c, (enum vga_color)c));
        terminal_writestring("  ");
    }
    terminal_writestring("\n");

    terminal_setcolor(lbl_color);
}

// ═══════════════════════════════════════════════════════════════════
// calc — simple integer calculator
// ═══════════════════════════════════════════════════════════════════

void cmd_calc(const char* args) {
    if (!args || !*args) {
        terminal_writestring("Usage: calc <expr>  (e.g. calc 10 + 3)\n");
        terminal_writestring("Operators: + - * / %\n");
        return;
    }

    // Parse: number op number
    int i = 0;
    int neg1 = 0;

    // Skip spaces
    while (args[i] == ' ') i++;
    if (args[i] == '-') { neg1 = 1; i++; }

    long a = 0;
    if (args[i] < '0' || args[i] > '9') {
        terminal_writestring("Error: Expected number\n");
        return;
    }
    while (args[i] >= '0' && args[i] <= '9') { a = a * 10 + (args[i] - '0'); i++; }
    if (neg1) a = -a;

    while (args[i] == ' ') i++;

    char op = args[i++];
    if (op != '+' && op != '-' && op != '*' && op != '/' && op != '%') {
        terminal_writestring("Error: Unknown operator '");
        terminal_putchar(op);
        terminal_writestring("'\n");
        return;
    }

    while (args[i] == ' ') i++;
    int neg2 = 0;
    if (args[i] == '-') { neg2 = 1; i++; }

    long b = 0;
    if (args[i] < '0' || args[i] > '9') {
        terminal_writestring("Error: Expected number\n");
        return;
    }
    while (args[i] >= '0' && args[i] <= '9') { b = b * 10 + (args[i] - '0'); i++; }
    if (neg2) b = -b;

    long result;
    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/':
            if (b == 0) { terminal_writestring("Error: Division by zero\n"); return; }
            result = a / b;
            break;
        case '%':
            if (b == 0) { terminal_writestring("Error: Division by zero\n"); return; }
            result = a % b;
            break;
        default: return;
    }

    uint8_t val = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t lbl = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(lbl);
    terminal_writestring("= ");
    terminal_setcolor(val);

    if (result < 0) {
        terminal_putchar('-');
        result = -result;
    }
    if (result == 0) {
        terminal_putchar('0');
    } else {
        char digits[20];
        int d = 0;
        long tmp = result;
        while (tmp > 0) { digits[d++] = '0' + (int)(tmp % 10); tmp /= 10; }
        while (d > 0) terminal_putchar(digits[--d]);
    }

    terminal_setcolor(lbl);
    terminal_writestring("\n");
}

