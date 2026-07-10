#ifndef DECODER_H
#define DECODER_H

struct exception_info
{
    const char *exception_class;
    const char *fault_status;
    const char *access_type;
    unsigned long ec;
    unsigned long iss;
    unsigned long fsc;
    int fault_level;
    int has_fault_status;
};

const char *decode_exception_class(unsigned long esr);
const char *decode_fault_status(unsigned long fsc);
void decode_exception_info(unsigned long esr, struct exception_info *info);

#endif
