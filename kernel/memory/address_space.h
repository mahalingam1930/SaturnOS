#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

enum address_space_kind
{
    ADDRESS_SPACE_KERNEL = 0,
    ADDRESS_SPACE_TASK,
};

struct address_space
{
    const char *name;
    enum address_space_kind kind;
    unsigned long root_table;
    int shared_kernel_map;
};

void address_space_init(unsigned long kernel_root_table);
struct address_space *address_space_kernel(void);
const char *address_space_kind_name(enum address_space_kind kind);

#endif
