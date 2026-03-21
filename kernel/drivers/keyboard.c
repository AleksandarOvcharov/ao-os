#include "keyboard.h"
#include "io.h"

static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int extended_key = 0;

/* Ring buffer for interrupt-driven keyboard input */
#define KB_BUFFER_SIZE 256
static unsigned char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

static unsigned char translate_scancode(unsigned char scancode);

void keyboard_init(void) {
    shift_pressed = 0;
    ctrl_pressed = 0;
    extended_key = 0;
    kb_head = 0;
    kb_tail = 0;
}

/* Called from IRQ1 handler in interrupt.asm */
void keyboard_irq_handler(void) {
    unsigned char scancode = inb(0x60);
    unsigned char ch = translate_scancode(scancode);
    if (ch != 0) {
        int next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = ch;
            kb_head = next;
        }
    }
}

static unsigned char translate_scancode(unsigned char scancode) {
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

    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return 0;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
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
            case 0x49: return KEY_PAGEUP;
            case 0x51: return KEY_PAGEDOWN;
            default: return 0;
        }
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char ch;
        if (shift_pressed) {
            ch = scancode_to_ascii_shift[scancode];
        } else {
            ch = scancode_to_ascii[scancode];
        }

        if (ctrl_pressed && ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 1;
        }
        if (ctrl_pressed && ch >= 'A' && ch <= 'Z') {
            return ch - 'A' + 1;
        }

        return ch;
    }

    return 0;
}

unsigned char keyboard_getchar(void) {
    /* Read from interrupt ring buffer */
    if (kb_tail != kb_head) {
        unsigned char ch = kb_buffer[kb_tail];
        kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
        return ch;
    }
    return 0;
}

void keyboard_wait_for_key(void) {
    while (1) {
        unsigned char c = keyboard_getchar();
        if (c != 0) {
            return;
        }
        asm volatile("hlt");
    }
}
