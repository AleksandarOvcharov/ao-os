#include "keyboard.h"
#include "io.h"

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

static int shift_pressed = 0;
static int extended_key = 0;

void keyboard_init(void) {
    shift_pressed = 0;
    extended_key = 0;
}

unsigned char keyboard_getchar(void) {
    unsigned char status;
    unsigned char scancode;
    
    status = inb(0x64);
    if (!(status & 0x01)) {
        return 0;
    }
    
    scancode = inb(0x60);
    
    if (scancode == 0xE0) {
        extended_key = 1;
        return 0;
    }
    
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }
    
    if (scancode & 0x80) {
        extended_key = 0;
        return 0;
    }
    
    if (extended_key) {
        extended_key = 0;
        switch (scancode) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default: return 0;
        }
    }
    
    if (scancode < sizeof(scancode_to_ascii)) {
        if (shift_pressed) {
            return scancode_to_ascii_shift[scancode];
        } else {
            return scancode_to_ascii[scancode];
        }
    }
    
    return 0;
}

void keyboard_wait_for_key(void) {
    while (1) {
        unsigned char c = keyboard_getchar();
        if (c != 0) {
            return;
        }
    }
}
