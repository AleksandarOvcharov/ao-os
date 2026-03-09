#include "ao.h"

void _start(void) {
    // Set color to cyan
    ao_setcolor(COLOR_LIGHT_CYAN, COLOR_BLACK);
    
    // Print header
    ao_println("=================================");
    ao_println("  Hello from C Program!");
    ao_println("=================================");
    
    // Change color to green
    ao_setcolor(COLOR_LIGHT_GREEN, COLOR_BLACK);
    ao_print("This program was written in C\n");
    
    // Print some numbers
    ao_setcolor(COLOR_YELLOW, COLOR_BLACK);
    ao_print("Counting: ");
    for (int i = 1; i <= 5; i++) {
        ao_printint(i);
        ao_print(" ");
    }
    ao_print("\n");
    
    // Reset color
    ao_setcolor(COLOR_LIGHT_GREY, COLOR_BLACK);
    ao_println("Program finished!");
    
    // Return to kernel
    return;
}
