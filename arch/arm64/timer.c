#include "timer.h"

static unsigned long timer_frequency;

void timer_init(void)
{
    __asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(timer_frequency));
}

unsigned long timer_get_frequency(void)
{
    return timer_frequency;
}

unsigned long timer_get_ticks(void)
{
    unsigned long ticks;

    __asm__ volatile("isb");
    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(ticks));

    return ticks;
}

void timer_sleep_ms(unsigned long ms)
{
    unsigned long start = timer_get_ticks();
    unsigned long ticks_per_ms = timer_frequency / 1000;
    unsigned long delay_ticks = ticks_per_ms * ms;

    while ((timer_get_ticks() - start) < delay_ticks)
    {
    }
}
