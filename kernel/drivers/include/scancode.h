#ifndef SCANCODE_H
#define SCANCODE_H

#include <stdint.h>

/* Translate PS/2 scancode to ASCII or special key code.
 * Handles shift, ctrl, and extended key state internally. */
unsigned char translate_scancode(unsigned char scancode);

#endif
