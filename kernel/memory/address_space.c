#include "address_space.h"

static struct address_space kernel_address_space;

void address_space_init(unsigned long kernel_root_table)
{
    kernel_address_space.name = "kernel";
    kernel_address_space.kind = ADDRESS_SPACE_KERNEL;
    kernel_address_space.root_table = kernel_root_table;
    kernel_address_space.shared_kernel_map = 1;
}

struct address_space *address_space_kernel(void)
{
    return &kernel_address_space;
}

const char *address_space_kind_name(enum address_space_kind kind)
{
    switch (kind)
    {
        case ADDRESS_SPACE_KERNEL:
            return "kernel";
        case ADDRESS_SPACE_TASK:
            return "task";
        default:
            return "unknown";
    }
}
