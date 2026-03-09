#include "ao.h"

void _start(void) {
    ao_setcolor(COLOR_LIGHT_CYAN, COLOR_BLACK);
    ao_println("=================================");
    ao_println("  Hello from C Program!");
    ao_println("=================================");

    // Print arguments
    int argc = ao_argc();
    ao_setcolor(COLOR_YELLOW, COLOR_BLACK);
    ao_print("Arguments: ");
    ao_putchar('0' + argc);
    ao_putchar('\n');

    for (int i = 0; i < argc; i++) {
        ao_print("  argv[");
        ao_putchar('0' + i);
        ao_print("] = ");
        ao_println(ao_argv(i));
    }

    ao_setcolor(COLOR_LIGHT_GREY, COLOR_BLACK);
    ao_println("Press any key to exit...");
    ao_getchar();

    ao_exit(0);
}
