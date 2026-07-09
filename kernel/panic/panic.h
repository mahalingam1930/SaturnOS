#ifndef PANIC_H
#define PANIC_H

void kernel_panic(const char *reason,
                  unsigned long esr,
                  unsigned long elr,
                  unsigned long far,
                  unsigned long spsr);

#endif