/* PS/2 keyboard IRQ handler and input buffer */
#include "keyboard.h"
#include "io.h"
#include "scancode.h"

/* Ring buffer for interrupt-driven keyboard input */
#define KB_BUFFER_SIZE 256
static unsigned char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

/* Timeout for PS/2 controller waits */
#define PS2_TIMEOUT 100000

static int ps2_wait_input(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (!(inb(0x64) & 0x02)) return 0;
    }
    return -1;
}

static int ps2_wait_output(void) {
    for (int i = 0; i < PS2_TIMEOUT; i++) {
        if (inb(0x64) & 0x01) return 0;
    }
    return -1;
}

void keyboard_init(void) {
    kb_head = 0;
    kb_tail = 0;

    /* Drain any stale data from the PS/2 controller output buffer.
     * If the buffer is full, the controller won't raise new IRQ1s. */
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    /* Enable the keyboard interface (in case BIOS disabled it) */
    if (ps2_wait_input() == 0)
        outb(0x64, 0xAE);

    /* Try to read and update controller config to enable IRQ1 */
    if (ps2_wait_input() == 0) {
        outb(0x64, 0x20);
        if (ps2_wait_output() == 0) {
            unsigned char config = inb(0x60);
            config |= 0x01;         /* enable IRQ1 for port 1 */
            config &= ~0x10;        /* enable keyboard clock */
            if (ps2_wait_input() == 0) {
                outb(0x64, 0x60);
                if (ps2_wait_input() == 0)
                    outb(0x60, config);
            }
        }
    }

    /* Drain again after configuration changes */
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
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

unsigned char keyboard_getchar(void) {
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
