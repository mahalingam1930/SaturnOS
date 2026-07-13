#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#define ADDRESS_SPACE_USER_REGION_COUNT 3UL

enum address_space_kind
{
    ADDRESS_SPACE_KERNEL = 0,
    ADDRESS_SPACE_USER,
};

struct address_space_user_region
{
    const char *name;
    unsigned long start;
    unsigned long end;
    unsigned long attributes;
    int user_access;
    int writable;
    int executable;
};

struct address_space
{
    const char *name;
    enum address_space_kind kind;
    unsigned long root_table;
    unsigned long kernel_root_table;
    unsigned long user_root_table;
    unsigned long user_table_count;
    unsigned long user_start;
    unsigned long user_end;
    unsigned long user_code_start;
    unsigned long user_code_end;
    unsigned long user_data_start;
    unsigned long user_data_end;
    unsigned long user_stack_start;
    unsigned long user_stack_end;
    unsigned long user_mapping_count;
    unsigned long user_descriptor_count;
    struct address_space_user_region
        user_regions[ADDRESS_SPACE_USER_REGION_COUNT];
    int shared_kernel_map;
    int user_tables_ready;
    int user_descriptors_ready;
    int user_mappings_ready;
    int user_execute_ready;
};

void address_space_init(unsigned long kernel_root_table);
void address_space_init_user(struct address_space *space,
                             const char *name,
                             unsigned long kernel_root_table);
struct address_space *address_space_kernel(void);
const char *address_space_kind_name(enum address_space_kind kind);

#endif
