#ifndef EXCEPTION_H
#define EXCEPTION_H

void exception_test(void);
void exception_test_page_fault(void);

void exception_handler(void);

/* Defined in exception.S */
extern void exception_vector(void);

/* Initialize exception handling */
void exception_init(void);

#endif
