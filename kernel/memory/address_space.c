#include "address_space.h"

static struct address_space kernel_address_space;

void address_space_init(unsigned long kernel_root_table)
{
    kernel_address_space.name = "kernel";
    kernel_address_space.kind = ADDRESS_SPACE_KERNEL;
    kernel_address_space.root_table = kernel_root_table;
    kernel_address_space.user_start = 0;
    kernel_address_space.user_end = 0;
    kernel_address_space.shared_kernel_map = 1;
    kernel_address_space.user_mappings_ready = 0;
}

void address_space_init_user(struct address_space *space,
                             const char *name,
                             unsigned long root_table)
{
    if (!space)
    {
        return;
    }

    space->name = name ? name : "user";
    space->kind = ADDRESS_SPACE_USER;
    space->root_table = root_table;
    space->user_start = 0x00010000;
    space->user_end = 0x40000000;
    space->shared_kernel_map = 1;
    space->user_mappings_ready = 0;
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
        case ADDRESS_SPACE_USER:
            return "user";
        default:
            return "unknown";
    }
}
