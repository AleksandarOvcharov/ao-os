#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"

void kernel_main(void) {
    terminal_initialize();
    memory_init();
    keyboard_init();
    shell_init();
    shell_run();
    
    while (1) {
        asm volatile("hlt");
    }
}
