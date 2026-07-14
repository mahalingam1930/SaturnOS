#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#define ADDRESS_SPACE_USER_REGION_COUNT 3UL

enum address_space_kind
{
    ADDRESS_SPACE_KERNEL = 0,
    ADDRESS_SPACE_USER,
};

enum address_space_switch_status
{
    ADDRESS_SPACE_SWITCH_ACTIVE = 0,
    ADDRESS_SPACE_SWITCH_READY,
    ADDRESS_SPACE_SWITCH_BLOCKED,
};

enum address_space_switch_stub_status
{
    ADDRESS_SPACE_SWITCH_STUB_ACTIVE = 0,
    ADDRESS_SPACE_SWITCH_STUB_READY,
    ADDRESS_SPACE_SWITCH_STUB_BLOCKED,
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
    unsigned long active_root_table;
    unsigned long target_root_table;
    unsigned long user_table_slot;
    unsigned long user_table_count;
    unsigned long user_start;
    unsigned long user_end;
    unsigned long user_code_start;
    unsigned long user_code_end;
    unsigned long user_data_start;
    unsigned long user_data_end;
    unsigned long user_stack_start;
    unsigned long user_stack_end;
    unsigned long user_image_entry;
    unsigned long user_image_size;
    unsigned long user_image_checksum;
    unsigned long user_mapping_count;
    unsigned long user_descriptor_count;
    unsigned long user_installed_descriptor_count;
    struct address_space_user_region
        user_regions[ADDRESS_SPACE_USER_REGION_COUNT];
    int shared_kernel_map;
    int permission_split_ready;
    int kernel_el0_access;
    int user_el0_access;
    int user_tables_ready;
    int user_descriptors_ready;
    int user_mappings_ready;
    int user_image_ready;
    int user_execute_ready;
    int validation_ready;
    int switch_ready;
    enum address_space_switch_status switch_status;
    enum address_space_switch_stub_status switch_stub_status;
    unsigned long validation_errors;
};

void address_space_init(unsigned long kernel_root_table);
void address_space_init_user(struct address_space *space,
                             const char *name,
                             unsigned long kernel_root_table);
int address_space_install_user_smoke_image(struct address_space *space);
int address_space_user_range_valid(const struct address_space *space,
                                   unsigned long address,
                                   unsigned long size);
struct address_space *address_space_kernel(void);
const char *address_space_kind_name(enum address_space_kind kind);
const char *address_space_validation_state(const struct address_space *space);
const char *address_space_switch_state(const struct address_space *space);
const char *address_space_switch_stub_state(const struct address_space *space);
int address_space_switch_stub(struct address_space *space);

#endif
