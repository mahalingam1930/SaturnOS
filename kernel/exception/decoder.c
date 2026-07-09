// decoder.c
#include "decoder.h"

const char *decode_exception_class(unsigned long esr)
{
    unsigned long ec = (esr >> 26) & 0x3F;

    switch (ec)
    {
        case 0x15:
            return "SVC Instruction";
        case 0x20:
            return "Instruction Abort";
        case 0x24:
            return "Data Abort";
        case 0x3C:
            return "Breakpoint Instruction";
        default:
            return "Unknown Exception";
    }
}