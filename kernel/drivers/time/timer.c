#include "timer.h"
#include "io.h"

static volatile uint32_t system_ticks = 0;

static void timer_handler(void) {
    system_ticks++;
}

void timer_phase(uint32_t hz) {
    uint32_t divisor = PIT_FREQUENCY / hz;

    outb(0x43, 0x36);

    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_init(uint32_t frequency) {
    system_ticks = 0;
    timer_phase(frequency);
}

void timer_tick(void) {
    timer_handler();
}

uint32_t timer_get_ticks(void) {
    return system_ticks;
}

void timer_wait(uint32_t ticks) {
    uint32_t end_tick = system_ticks + ticks;
    while (system_ticks < end_tick) {
        asm volatile("hlt");
    }
}
