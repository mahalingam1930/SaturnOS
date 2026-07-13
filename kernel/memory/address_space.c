#include "address_space.h"
#include "mmu.h"

#define ADDRESS_SPACE_USER_TABLE_SLOTS 4UL
#define ADDRESS_SPACE_USER_START 0x00010000UL
#define ADDRESS_SPACE_USER_END 0x40000000UL
#define ADDRESS_SPACE_USER_CODE_START 0x00100000UL
#define ADDRESS_SPACE_USER_CODE_END 0x00200000UL
#define ADDRESS_SPACE_USER_DATA_START 0x00200000UL
#define ADDRESS_SPACE_USER_DATA_END 0x00300000UL
#define ADDRESS_SPACE_USER_STACK_START 0x3fff0000UL
#define ADDRESS_SPACE_USER_STACK_END 0x40000000UL

static struct address_space kernel_address_space;
static unsigned long user_l1_tables[ADDRESS_SPACE_USER_TABLE_SLOTS]
                                   [ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long user_table_slots_used;

static void address_space_clear_table(unsigned long *table)
{
    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        table[i] = 0;
    }
}

static unsigned long *address_space_alloc_user_l1_table(void)
{
    unsigned long slot;

    if (user_table_slots_used >= ADDRESS_SPACE_USER_TABLE_SLOTS)
    {
        return 0;
    }

    slot = user_table_slots_used++;
    address_space_clear_table(user_l1_tables[slot]);

    return user_l1_tables[slot];
}

void address_space_init(unsigned long kernel_root_table)
{
    kernel_address_space.name = "kernel";
    kernel_address_space.kind = ADDRESS_SPACE_KERNEL;
    kernel_address_space.root_table = kernel_root_table;
    kernel_address_space.kernel_root_table = kernel_root_table;
    kernel_address_space.user_root_table = 0;
    kernel_address_space.user_table_count = 0;
    kernel_address_space.user_start = 0;
    kernel_address_space.user_end = 0;
    kernel_address_space.user_code_start = 0;
    kernel_address_space.user_code_end = 0;
    kernel_address_space.user_data_start = 0;
    kernel_address_space.user_data_end = 0;
    kernel_address_space.user_stack_start = 0;
    kernel_address_space.user_stack_end = 0;
    kernel_address_space.user_mapping_count = 0;
    kernel_address_space.shared_kernel_map = 1;
    kernel_address_space.user_tables_ready = 0;
    kernel_address_space.user_mappings_ready = 0;
    kernel_address_space.user_execute_ready = 0;
}

void address_space_init_user(struct address_space *space,
                             const char *name,
                             unsigned long kernel_root_table)
{
    unsigned long *user_l1_table;

    if (!space)
    {
        return;
    }

    user_l1_table = address_space_alloc_user_l1_table();

    space->name = name ? name : "user";
    space->kind = ADDRESS_SPACE_USER;
    space->root_table = kernel_root_table;
    space->kernel_root_table = kernel_root_table;
    space->user_root_table = (unsigned long)user_l1_table;
    space->user_table_count = user_l1_table ? 1 : 0;
    space->user_start = ADDRESS_SPACE_USER_START;
    space->user_end = ADDRESS_SPACE_USER_END;
    space->user_code_start = ADDRESS_SPACE_USER_CODE_START;
    space->user_code_end = ADDRESS_SPACE_USER_CODE_END;
    space->user_data_start = ADDRESS_SPACE_USER_DATA_START;
    space->user_data_end = ADDRESS_SPACE_USER_DATA_END;
    space->user_stack_start = ADDRESS_SPACE_USER_STACK_START;
    space->user_stack_end = ADDRESS_SPACE_USER_STACK_END;
    space->user_mapping_count = 3;
    space->shared_kernel_map = 1;
    space->user_tables_ready = user_l1_table ? 1 : 0;
    space->user_mappings_ready = 0;
    space->user_execute_ready = 0;
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
