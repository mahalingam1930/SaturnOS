#ifndef PANIC_H
#define PANIC_H

#include "decoder.h"

void kernel_panic(const struct exception_info *info,
                  unsigned long esr,
                  unsigned long elr,
                  unsigned long far,
                  unsigned long spsr);

#endif
