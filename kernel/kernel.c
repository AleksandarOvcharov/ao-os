#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"
#include "idt.h"
#include "timer.h"

void kernel_main(void) {
    terminal_initialize();
    idt_init();
    timer_init(TIMER_HZ);
    memory_init();
    keyboard_init();
    
    asm volatile("sti");
    
    shell_init();
    shell_run();
    
    while (1) {
        asm volatile("hlt");
    }
}
