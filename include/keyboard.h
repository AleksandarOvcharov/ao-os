#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_UP      0x80
#define KEY_DOWN    0x81
#define KEY_LEFT    0x82
#define KEY_RIGHT   0x83
#define KEY_PAGEUP  0x84
#define KEY_PAGEDOWN 0x85

void keyboard_init(void);
unsigned char keyboard_getchar(void);
void keyboard_wait_for_key(void);

#endif
