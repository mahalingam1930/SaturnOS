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
#define ADDRESS_SPACE_USER_CODE_ATTR \
    ((ARM64_NORMAL_MEMORY_ATTR & ~ARM64_DESC_AP_MASK) | \
     ARM64_DESC_AP_RO_EL0 | \
     ARM64_DESC_PXN)
#define ADDRESS_SPACE_USER_DATA_ATTR \
    ((ARM64_NORMAL_MEMORY_ATTR & ~ARM64_DESC_AP_MASK) | \
     ARM64_DESC_AP_RW_EL0 | \
     ARM64_DESC_PXN | \
     ARM64_DESC_UXN)
#define ADDRESS_SPACE_USER_STACK_ATTR ADDRESS_SPACE_USER_DATA_ATTR

static struct address_space kernel_address_space;
static unsigned long user_l1_tables[ADDRESS_SPACE_USER_TABLE_SLOTS]
                                   [ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long user_l2_tables[ADDRESS_SPACE_USER_TABLE_SLOTS]
                                   [ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned long user_l3_tables[ADDRESS_SPACE_USER_TABLE_SLOTS]
                                   [ADDRESS_SPACE_USER_REGION_COUNT]
                                   [ARM64_TABLE_ENTRIES]
    __attribute__((aligned(ARM64_TABLE_SIZE)));
static unsigned char user_region_pages[ADDRESS_SPACE_USER_TABLE_SLOTS]
                                      [ADDRESS_SPACE_USER_REGION_COUNT]
                                      [ARM64_PAGE_SIZE]
    __attribute__((aligned(ARM64_PAGE_SIZE)));
static unsigned long user_table_slots_used;

static void address_space_clear_table(unsigned long *table)
{
    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        table[i] = 0;
    }
}

static int address_space_alloc_user_table_slot(void)
{
    unsigned long slot;

    if (user_table_slots_used >= ADDRESS_SPACE_USER_TABLE_SLOTS)
    {
        return -1;
    }

    slot = user_table_slots_used++;
    address_space_clear_table(user_l1_tables[slot]);
    address_space_clear_table(user_l2_tables[slot]);
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        address_space_clear_table(user_l3_tables[slot][i]);
    }

    return (int)slot;
}

static unsigned long address_space_install_user_descriptors(
    struct address_space *space)
{
    unsigned long installed = 0;
    unsigned long *l1_table;
    unsigned long *l2_table;
    unsigned long slot;

    if (!space || space->user_table_slot >= ADDRESS_SPACE_USER_TABLE_SLOTS)
    {
        return 0;
    }

    slot = space->user_table_slot;
    l1_table = user_l1_tables[slot];
    l2_table = user_l2_tables[slot];

    address_space_clear_table(l1_table);
    address_space_clear_table(l2_table);
    l1_table[0] = arm64_mmu_table_desc((unsigned long)l2_table);

    for (unsigned long i = 0; i < space->user_descriptor_count; i++)
    {
        unsigned long l2_index;
        unsigned long l3_index;
        unsigned long *l3_table;

        if (i >= ADDRESS_SPACE_USER_REGION_COUNT ||
            !space->user_regions[i].name)
        {
            continue;
        }

        l2_index = arm64_mmu_l2_index(space->user_regions[i].start);
        l3_index = arm64_mmu_l3_index(space->user_regions[i].start);
        l3_table = user_l3_tables[slot][i];

        address_space_clear_table(l3_table);
        l2_table[l2_index] = arm64_mmu_table_desc((unsigned long)l3_table);
        l3_table[l3_index] =
            arm64_mmu_l3_page_desc(
                (unsigned long)&user_region_pages[slot][i][0],
                space->user_regions[i].attributes);
        installed++;
    }

    return installed;
}

static void address_space_clear_user_regions(struct address_space *space)
{
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        space->user_regions[i].name = 0;
        space->user_regions[i].start = 0;
        space->user_regions[i].end = 0;
        space->user_regions[i].attributes = 0;
        space->user_regions[i].user_access = 0;
        space->user_regions[i].writable = 0;
        space->user_regions[i].executable = 0;
    }
}

static void address_space_set_user_region(struct address_space *space,
                                          unsigned long index,
                                          const char *name,
                                          unsigned long start,
                                          unsigned long end,
                                          unsigned long attributes,
                                          int writable,
                                          int executable)
{
    if (index >= ADDRESS_SPACE_USER_REGION_COUNT)
    {
        return;
    }

    space->user_regions[index].name = name;
    space->user_regions[index].start = start;
    space->user_regions[index].end = end;
    space->user_regions[index].attributes = attributes;
    space->user_regions[index].user_access = 1;
    space->user_regions[index].writable = writable;
    space->user_regions[index].executable = executable;
}

static int address_space_region_is_valid(
    const struct address_space_user_region *region,
    int want_writable,
    int want_executable)
{
    if (!region->name)
    {
        return 0;
    }

    if (!region->user_access)
    {
        return 0;
    }

    if (region->start >= region->end)
    {
        return 0;
    }

    if (region->writable != want_writable)
    {
        return 0;
    }

    if (region->executable != want_executable)
    {
        return 0;
    }

    return 1;
}

static unsigned long address_space_validate_user(const struct address_space *space)
{
    unsigned long errors = 0;

    if (!space)
    {
        return 1;
    }

    if (!space->permission_split_ready ||
        space->kernel_el0_access ||
        !space->user_el0_access)
    {
        errors++;
    }

    if (!space->user_tables_ready ||
        !space->user_descriptors_ready ||
        !space->user_mappings_ready)
    {
        errors++;
    }

    if (!space->user_root_table ||
        space->user_table_slot >= ADDRESS_SPACE_USER_TABLE_SLOTS ||
        space->user_table_count != 5)
    {
        errors++;
    }

    if (space->user_descriptor_count != ADDRESS_SPACE_USER_REGION_COUNT ||
        space->user_installed_descriptor_count !=
            space->user_descriptor_count ||
        space->user_mapping_count != ADDRESS_SPACE_USER_REGION_COUNT)
    {
        errors++;
    }

    if (!address_space_region_is_valid(&space->user_regions[0], 0, 1) ||
        !address_space_region_is_valid(&space->user_regions[1], 1, 0) ||
        !address_space_region_is_valid(&space->user_regions[2], 1, 0))
    {
        errors++;
    }

    if (space->user_code_start < space->user_start ||
        space->user_stack_end > space->user_end ||
        space->user_code_start >= space->user_code_end ||
        space->user_data_start >= space->user_data_end ||
        space->user_stack_start >= space->user_stack_end)
    {
        errors++;
    }

    if (!space->user_execute_ready)
    {
        errors++;
    }

    return errors;
}

void address_space_init(unsigned long kernel_root_table)
{
    kernel_address_space.name = "kernel";
    kernel_address_space.kind = ADDRESS_SPACE_KERNEL;
    kernel_address_space.root_table = kernel_root_table;
    kernel_address_space.kernel_root_table = kernel_root_table;
    kernel_address_space.user_root_table = 0;
    kernel_address_space.user_table_slot = ADDRESS_SPACE_USER_TABLE_SLOTS;
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
    kernel_address_space.user_descriptor_count = 0;
    kernel_address_space.user_installed_descriptor_count = 0;
    address_space_clear_user_regions(&kernel_address_space);
    kernel_address_space.shared_kernel_map = 1;
    kernel_address_space.permission_split_ready = 1;
    kernel_address_space.kernel_el0_access = 0;
    kernel_address_space.user_el0_access = 0;
    kernel_address_space.user_tables_ready = 0;
    kernel_address_space.user_descriptors_ready = 0;
    kernel_address_space.user_mappings_ready = 0;
    kernel_address_space.user_execute_ready = 0;
    kernel_address_space.validation_ready = 1;
    kernel_address_space.validation_errors = 0;
}

void address_space_init_user(struct address_space *space,
                             const char *name,
                             unsigned long kernel_root_table)
{
    int user_table_slot;

    if (!space)
    {
        return;
    }

    user_table_slot = address_space_alloc_user_table_slot();

    space->name = name ? name : "user";
    space->kind = ADDRESS_SPACE_USER;
    space->root_table = kernel_root_table;
    space->kernel_root_table = kernel_root_table;
    space->user_root_table =
        user_table_slot >= 0 ? (unsigned long)user_l1_tables[user_table_slot] :
        0;
    space->user_table_slot =
        user_table_slot >= 0 ? (unsigned long)user_table_slot :
        ADDRESS_SPACE_USER_TABLE_SLOTS;
    space->user_table_count = user_table_slot >= 0 ? 5 : 0;
    space->user_start = ADDRESS_SPACE_USER_START;
    space->user_end = ADDRESS_SPACE_USER_END;
    space->user_code_start = ADDRESS_SPACE_USER_CODE_START;
    space->user_code_end = ADDRESS_SPACE_USER_CODE_END;
    space->user_data_start = ADDRESS_SPACE_USER_DATA_START;
    space->user_data_end = ADDRESS_SPACE_USER_DATA_END;
    space->user_stack_start = ADDRESS_SPACE_USER_STACK_START;
    space->user_stack_end = ADDRESS_SPACE_USER_STACK_END;
    space->user_mapping_count = 3;
    space->user_descriptor_count = ADDRESS_SPACE_USER_REGION_COUNT;
    space->user_installed_descriptor_count = 0;
    address_space_clear_user_regions(space);
    address_space_set_user_region(space,
                                  0,
                                  "code",
                                  space->user_code_start,
                                  space->user_code_end,
                                  ADDRESS_SPACE_USER_CODE_ATTR,
                                  0,
                                  1);
    address_space_set_user_region(space,
                                  1,
                                  "data",
                                  space->user_data_start,
                                  space->user_data_end,
                                  ADDRESS_SPACE_USER_DATA_ATTR,
                                  1,
                                  0);
    address_space_set_user_region(space,
                                  2,
                                  "stack",
                                  space->user_stack_start,
                                  space->user_stack_end,
                                  ADDRESS_SPACE_USER_STACK_ATTR,
                                  1,
                                  0);
    space->shared_kernel_map = 1;
    space->permission_split_ready = 1;
    space->kernel_el0_access = 0;
    space->user_el0_access = 1;
    space->user_tables_ready = user_table_slot >= 0 ? 1 : 0;
    space->user_descriptors_ready = user_table_slot >= 0 ? 1 : 0;
    space->user_installed_descriptor_count =
        address_space_install_user_descriptors(space);
    space->user_mappings_ready =
        space->user_installed_descriptor_count == space->user_descriptor_count;
    space->user_execute_ready = space->user_mappings_ready;
    space->validation_errors = address_space_validate_user(space);
    space->validation_ready = space->validation_errors == 0;
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

const char *address_space_validation_state(const struct address_space *space)
{
    if (!space || !space->validation_ready || space->validation_errors)
    {
        return "errors";
    }

    return "ok";
}
