#include "timer.h"
#include "scheduler.h"

static unsigned long timer_frequency;
static unsigned long timer_interval_ticks;
static unsigned long timer_irq_ticks;

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

unsigned long timer_get_irq_ticks(void)
{
    return timer_irq_ticks;
}

void timer_start_periodic(unsigned long interval_ms)
{
    timer_interval_ticks = (timer_frequency / 1000) * interval_ms;
    timer_irq_ticks = 0;

    __asm__ volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_interval_ticks));
    __asm__ volatile("msr CNTP_CTL_EL0, %0" : : "r"(1UL));
}

void timer_handle_irq(void)
{
    timer_irq_ticks++;

    scheduler_tick();

    __asm__ volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_interval_ticks));
}
