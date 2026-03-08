#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define COM1 0x3F8

void serial_init(void);
void serial_putchar(char c);
void serial_writestring(const char* str);
int serial_is_transmit_empty(void);
int serial_is_initialized(void);

#endif
