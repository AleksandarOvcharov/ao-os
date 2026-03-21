#include "serial.h"
#include "io.h"

static int serial_initialized = 0;

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    serial_initialized = 1;
}

int serial_is_initialized(void) {
    return serial_initialized;
}

int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_writestring(const char* str) {
    while (*str) {
        serial_putchar(*str);
        str++;
    }
}
