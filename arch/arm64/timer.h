#ifndef TIMER_H
#define TIMER_H

void timer_init(void);
unsigned long timer_get_frequency(void);
unsigned long timer_get_ticks(void);
void timer_sleep_ms(unsigned long ms);

#endif
