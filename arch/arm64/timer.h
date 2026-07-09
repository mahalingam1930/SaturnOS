#ifndef TIMER_H
#define TIMER_H

void timer_init(void);
unsigned long timer_get_frequency(void);
unsigned long timer_get_ticks(void);
unsigned long timer_get_irq_ticks(void);
void timer_sleep_ms(unsigned long ms);
void timer_start_periodic(unsigned long interval_ms);
void timer_handle_irq(void);

#endif
