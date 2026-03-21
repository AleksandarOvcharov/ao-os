#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define PIT_FREQUENCY 1193182
#define TIMER_HZ 100

void timer_init(uint32_t frequency);
void timer_tick(void);
uint32_t timer_get_ticks(void);
void timer_wait(uint32_t ticks);

#endif
