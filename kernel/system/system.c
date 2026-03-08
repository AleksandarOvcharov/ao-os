#include "system.h"
#include "io.h"
#include "vga.h"

void system_reboot(void) {
    uint8_t temp;
    
    asm volatile("cli");
    
    do {
        temp = inb(0x64);
        if (temp & 0x01)
            inb(0x60);
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);
    
    asm volatile("hlt");
}

void system_shutdown(void) {
    terminal_writestring("\nShutting down AO OS...\n");
    terminal_writestring("It is now safe to turn off your computer.\n");
    
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    
    asm volatile("cli; hlt");
    
    while(1) {
        asm volatile("hlt");
    }
}
