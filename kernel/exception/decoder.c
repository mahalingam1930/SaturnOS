#include "decoder.h"

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3FUL
#define ESR_ISS_MASK 0x01ffffffUL
#define ESR_FSC_MASK 0x3FUL
#define ESR_WNR (1UL << 6)

static unsigned long exception_class_value(unsigned long esr)
{
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
}

const char *decode_exception_class(unsigned long esr)
{
    unsigned long ec = exception_class_value(esr);

    switch (ec)
    {
        case 0x15:
            return "SVC Instruction";
        case 0x20:
            return "Instruction Abort from lower EL";
        case 0x21:
            return "Instruction Abort from same EL";
        case 0x24:
            return "Data Abort from lower EL";
        case 0x25:
            return "Data Abort from same EL";
        case 0x3C:
            return "Breakpoint Instruction";
        default:
            return "Unknown Exception";
    }
}

const char *decode_fault_status(unsigned long fsc)
{
    switch (fsc)
    {
        case 0x04:
            return "Translation Fault, level 0";
        case 0x05:
            return "Translation Fault, level 1";
        case 0x06:
            return "Translation Fault, level 2";
        case 0x07:
            return "Translation Fault, level 3";
        case 0x09:
            return "Access Flag Fault, level 1";
        case 0x0a:
            return "Access Flag Fault, level 2";
        case 0x0b:
            return "Access Flag Fault, level 3";
        case 0x0d:
            return "Permission Fault, level 1";
        case 0x0e:
            return "Permission Fault, level 2";
        case 0x0f:
            return "Permission Fault, level 3";
        case 0x21:
            return "Alignment Fault";
        default:
            return "Unknown Fault Status";
    }
}

void decode_exception_info(unsigned long esr, struct exception_info *info)
{
    unsigned long ec = exception_class_value(esr);

    info->exception_class = decode_exception_class(esr);
    info->fault_status = "Not Applicable";
    info->access_type = "Not Applicable";
    info->ec = ec;
    info->iss = esr & ESR_ISS_MASK;
    info->fsc = 0;
    info->fault_level = -1;
    info->has_fault_status = 0;

    if (ec == 0x20 || ec == 0x21 || ec == 0x24 || ec == 0x25)
    {
        info->fsc = info->iss & ESR_FSC_MASK;
        info->fault_status = decode_fault_status(info->fsc);
        info->fault_level = (int)(info->fsc & 0x3);
        info->has_fault_status = 1;

        if (ec == 0x20 || ec == 0x21)
        {
            info->access_type = "Instruction Fetch";
        }
        else if (info->iss & ESR_WNR)
        {
            info->access_type = "Write";
        }
        else
        {
            info->access_type = "Read";
        }
    }
}
