#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "memory.h"
#include "idt.h"
#include "timer.h"
#include "klog.h"
#include "serial.h"
#include "ata.h"
#include "fs.h"
#include "syscall.h"

void kernel_main(void) {
    terminal_initialize();
    serial_init();
    
    klog_info("AO OS Kernel starting...");
    
    klog_info("Initializing IDT...");
    idt_init();
    syscall_init();
    
    klog_info("Initializing PIT timer...");
    timer_init(TIMER_HZ);
    
    asm volatile("sti");
    
    timer_wait(30);
    
    klog_info("Initializing memory manager...");
    memory_init();
    timer_wait(30);
    
    klog_info("Initializing ATA disk driver...");
    ata_init();
    timer_wait(30);
    
    klog_info("Initializing filesystem...");
    fs_init();
    timer_wait(30);
    
    klog_info("Initializing keyboard driver...");
    keyboard_init();
    timer_wait(30);
    
    klog_info("Kernel initialization complete!");
    timer_wait(50);
    terminal_writestring("\n");
    
    shell_init();
    shell_run();
    
    while (1) {
        asm volatile("hlt");
    }
}
