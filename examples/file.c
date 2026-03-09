#include "ao.h"

void _start(void) {
    ao_setcolor(COLOR_LIGHT_CYAN, COLOR_BLACK);
    ao_println("=================================");
    ao_println("  Hello from C Program!");
    ao_println("=================================");

    ao_setcolor(COLOR_LIGHT_GREEN, COLOR_BLACK);
    ao_println("This program was written in C");

    ao_setcolor(COLOR_YELLOW, COLOR_BLACK);
    ao_print("Enter your name: ");

    char name[64];
    ao_readline(name, sizeof(name));

    ao_setcolor(COLOR_WHITE, COLOR_BLACK);
    ao_print("Hello, ");
    ao_print(name);
    ao_println("!");

    ao_setcolor(COLOR_LIGHT_GREY, COLOR_BLACK);
    ao_println("Press any key to exit...");
    ao_getchar();

    ao_exit(0);
}
