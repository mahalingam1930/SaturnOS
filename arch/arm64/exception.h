#ifndef EXCEPTION_H
#define EXCEPTION_H

void exception_test(void);
void exception_test_page_fault(void);

struct exception_frame
{
    unsigned long x30;
    unsigned long pad30;
    unsigned long x28;
    unsigned long x29;
    unsigned long x26;
    unsigned long x27;
    unsigned long x24;
    unsigned long x25;
    unsigned long x22;
    unsigned long x23;
    unsigned long x20;
    unsigned long x21;
    unsigned long x18;
    unsigned long x19;
    unsigned long x16;
    unsigned long x17;
    unsigned long x14;
    unsigned long x15;
    unsigned long x12;
    unsigned long x13;
    unsigned long x10;
    unsigned long x11;
    unsigned long x8;
    unsigned long x9;
    unsigned long x6;
    unsigned long x7;
    unsigned long x4;
    unsigned long x5;
    unsigned long x2;
    unsigned long x3;
    unsigned long x0;
    unsigned long x1;
};

void exception_handler(void);
void exception_sync_handler(struct exception_frame *frame);

/* Defined in exception.S */
extern void exception_vector(void);

/* Initialize exception handling */
void exception_init(void);

#endif
