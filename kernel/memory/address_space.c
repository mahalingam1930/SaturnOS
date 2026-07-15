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
static unsigned char user_table_slots_used[ADDRESS_SPACE_USER_TABLE_SLOTS];

static void address_space_clear_table(unsigned long *table)
{
    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        table[i] = 0;
    }
}

static void address_space_clear_page(unsigned char *page)
{
    for (unsigned long i = 0; i < ARM64_PAGE_SIZE; i++)
    {
        page[i] = 0;
    }
}

static unsigned long address_space_checksum_bytes(const unsigned char *bytes,
                                                  unsigned long size)
{
    unsigned long checksum = 0;

    for (unsigned long i = 0; i < size; i++)
    {
        checksum = (checksum << 5) ^ (checksum >> 2) ^ bytes[i];
    }

    return checksum;
}

static int address_space_install_shared_kernel_map(
    struct address_space *space,
    unsigned long *l1_table,
    unsigned long *l2_table)
{
    unsigned long *kernel_l1_table;
    unsigned long *kernel_l2_table;
    unsigned long kernel_l1_descriptor;

    if (!space || !space->kernel_root_table)
    {
        return 0;
    }

    kernel_l1_table = (unsigned long *)space->kernel_root_table;
    kernel_l1_descriptor = kernel_l1_table[0];

    if (!arm64_mmu_desc_is_table(kernel_l1_descriptor))
    {
        return 0;
    }

    kernel_l2_table =
        (unsigned long *)arm64_mmu_desc_address(kernel_l1_descriptor);

    for (unsigned long i = 0; i < ARM64_TABLE_ENTRIES; i++)
    {
        l2_table[i] = kernel_l2_table[i];
    }

    l1_table[0] = arm64_mmu_table_desc((unsigned long)l2_table);

    for (unsigned long i = 1; i < ARM64_TABLE_ENTRIES; i++)
    {
        l1_table[i] = kernel_l1_table[i];
    }

    return 1;
}

static int address_space_alloc_user_table_slot(void)
{
    unsigned long slot;

    for (slot = 0; slot < ADDRESS_SPACE_USER_TABLE_SLOTS; slot++)
    {
        if (!user_table_slots_used[slot])
        {
            break;
        }
    }
    if (slot >= ADDRESS_SPACE_USER_TABLE_SLOTS)
    {
        return -1;
    }

    user_table_slots_used[slot] = 1;
    address_space_clear_table(user_l1_tables[slot]);
    address_space_clear_table(user_l2_tables[slot]);
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        address_space_clear_table(user_l3_tables[slot][i]);
        address_space_clear_page(user_region_pages[slot][i]);
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
    space->shared_kernel_map =
        address_space_install_shared_kernel_map(space, l1_table, l2_table);

    if (!space->shared_kernel_map)
    {
        l1_table[0] = arm64_mmu_table_desc((unsigned long)l2_table);
    }

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

int address_space_user_range_valid(const struct address_space *space,
                                   unsigned long address,
                                   unsigned long size)
{
    unsigned long end;

    if (!space || space->kind != ADDRESS_SPACE_USER)
    {
        return 0;
    }

    if (size == 0)
    {
        return 1;
    }

    end = address + size;
    if (end < address)
    {
        return 0;
    }

    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        const struct address_space_user_region *region =
            &space->user_regions[i];

        if (!region->name || !region->user_access)
        {
            continue;
        }

        if (address >= region->start && end <= region->end)
        {
            return 1;
        }
    }

    return 0;
}

int address_space_user_writable_range_valid(const struct address_space *space,
                                            unsigned long address,
                                            unsigned long size)
{
    unsigned long end;

    if (!space || space->kind != ADDRESS_SPACE_USER || size == 0)
    {
        return size == 0 && space && space->kind == ADDRESS_SPACE_USER;
    }
    end = address + size;
    if (end < address)
    {
        return 0;
    }
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        const struct address_space_user_region *region =
            &space->user_regions[i];
        if (region->name && region->user_access && region->writable &&
            address >= region->start && end <= region->end)
        {
            return 1;
        }
    }
    return 0;
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
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_TABLE_SLOTS; i++)
    {
        user_table_slots_used[i] = 0;
    }

    kernel_address_space.name = "kernel";
    kernel_address_space.kind = ADDRESS_SPACE_KERNEL;
    kernel_address_space.root_table = kernel_root_table;
    kernel_address_space.kernel_root_table = kernel_root_table;
    kernel_address_space.user_root_table = 0;
    kernel_address_space.active_root_table = kernel_root_table;
    kernel_address_space.target_root_table = kernel_root_table;
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
    kernel_address_space.user_image_entry = 0;
    kernel_address_space.user_image_size = 0;
    kernel_address_space.user_image_checksum = 0;
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
    kernel_address_space.user_image_ready = 0;
    kernel_address_space.user_execute_ready = 0;
    kernel_address_space.validation_ready = 1;
    kernel_address_space.switch_ready = 1;
    kernel_address_space.switch_status = ADDRESS_SPACE_SWITCH_ACTIVE;
    kernel_address_space.switch_stub_status =
        ADDRESS_SPACE_SWITCH_STUB_ACTIVE;
    kernel_address_space.validation_errors = 0;
}

void address_space_destroy_user(struct address_space *space)
{
    unsigned long slot;

    if (!space || space->kind != ADDRESS_SPACE_USER ||
        space->user_table_slot >= ADDRESS_SPACE_USER_TABLE_SLOTS)
    {
        return;
    }

    slot = space->user_table_slot;
    address_space_clear_table(user_l1_tables[slot]);
    address_space_clear_table(user_l2_tables[slot]);
    for (unsigned long i = 0; i < ADDRESS_SPACE_USER_REGION_COUNT; i++)
    {
        address_space_clear_table(user_l3_tables[slot][i]);
        address_space_clear_page(user_region_pages[slot][i]);
    }
    user_table_slots_used[slot] = 0;
    space->user_table_slot = ADDRESS_SPACE_USER_TABLE_SLOTS;
    space->user_tables_ready = 0;
    space->user_descriptors_ready = 0;
    space->user_mappings_ready = 0;
    space->user_image_ready = 0;
    space->user_execute_ready = 0;
    space->validation_ready = 0;
    space->switch_ready = 0;
    space->switch_status = ADDRESS_SPACE_SWITCH_BLOCKED;
    space->switch_stub_status = ADDRESS_SPACE_SWITCH_STUB_BLOCKED;
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
    space->active_root_table = kernel_root_table;
    space->target_root_table =
        user_table_slot >= 0 ? (unsigned long)user_l1_tables[user_table_slot] :
        kernel_root_table;
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
    space->user_image_entry = 0;
    space->user_image_size = 0;
    space->user_image_checksum = 0;
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
    space->shared_kernel_map = 0;
    space->permission_split_ready = 1;
    space->kernel_el0_access = 0;
    space->user_el0_access = 1;
    space->user_tables_ready = user_table_slot >= 0 ? 1 : 0;
    space->user_descriptors_ready = user_table_slot >= 0 ? 1 : 0;
    space->user_installed_descriptor_count =
        address_space_install_user_descriptors(space);
    space->user_mappings_ready =
        space->user_installed_descriptor_count == space->user_descriptor_count;
    space->user_image_ready = 0;
    space->user_execute_ready = space->user_mappings_ready;
    space->validation_errors = address_space_validate_user(space);
    space->validation_ready = space->validation_errors == 0;
    space->switch_ready =
        space->validation_ready &&
        space->target_root_table &&
        space->target_root_table != space->active_root_table;
    space->switch_status = space->switch_ready ?
        ADDRESS_SPACE_SWITCH_READY :
        ADDRESS_SPACE_SWITCH_BLOCKED;
    space->switch_stub_status = space->switch_ready ?
        ADDRESS_SPACE_SWITCH_STUB_READY :
        ADDRESS_SPACE_SWITCH_STUB_BLOCKED;
}

int address_space_load_user_image(struct address_space *space,
                                  const void *code,
                                  unsigned long code_size,
                                  const void *data,
                                  unsigned long data_size,
                                  unsigned long entry_offset)
{
    const unsigned char *code_bytes = (const unsigned char *)code;
    const unsigned char *data_bytes = (const unsigned char *)data;
    unsigned char *code_page;
    unsigned char *data_page;
    unsigned long slot;

    if (!space ||
        space->kind != ADDRESS_SPACE_USER ||
        space->user_table_slot >= ADDRESS_SPACE_USER_TABLE_SLOTS ||
        !space->user_mappings_ready ||
        !code ||
        code_size == 0 ||
        code_size > ARM64_PAGE_SIZE ||
        data_size > ARM64_PAGE_SIZE ||
        (data_size && !data) ||
        entry_offset >= code_size ||
        (entry_offset & (sizeof(unsigned int) - 1UL)))
    {
        return 0;
    }

    slot = space->user_table_slot;
    code_page = &user_region_pages[slot][0][0];
    data_page = &user_region_pages[slot][1][0];
    address_space_clear_page(code_page);
    address_space_clear_page(data_page);

    for (unsigned long i = 0; i < code_size; i++)
    {
        code_page[i] = code_bytes[i];
    }
    for (unsigned long i = 0; i < data_size; i++)
    {
        data_page[i] = data_bytes[i];
    }

    space->user_image_entry = space->user_code_start + entry_offset;
    space->user_image_size = code_size;
    space->user_image_checksum =
        address_space_checksum_bytes(code_page, code_size);
    space->user_image_ready = 1;
    return 1;
}

int address_space_write_user_data(struct address_space *space,
                                  unsigned long offset,
                                  const void *data,
                                  unsigned long size)
{
    unsigned long slot;
    unsigned char *data_page;

    if (!space || space->kind != ADDRESS_SPACE_USER ||
        space->user_table_slot >= ADDRESS_SPACE_USER_TABLE_SLOTS ||
        (size && !data) || offset > ARM64_PAGE_SIZE ||
        size > ARM64_PAGE_SIZE - offset)
    {
        return 0;
    }
    slot = space->user_table_slot;
    data_page = &user_region_pages[slot][1][0];
    for (unsigned long i = 0; i < size; i++)
    {
        data_page[offset + i] = ((const unsigned char *)data)[i];
    }
    return 1;
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

const char *address_space_switch_state(const struct address_space *space)
{
    if (!space)
    {
        return "blocked";
    }

    switch (space->switch_status)
    {
        case ADDRESS_SPACE_SWITCH_ACTIVE:
            return "active";
        case ADDRESS_SPACE_SWITCH_READY:
            return "ready";
        case ADDRESS_SPACE_SWITCH_BLOCKED:
            return "blocked";
        default:
            return "unknown";
    }
}

const char *address_space_switch_stub_state(const struct address_space *space)
{
    if (!space)
    {
        return "blocked";
    }

    switch (space->switch_stub_status)
    {
        case ADDRESS_SPACE_SWITCH_STUB_ACTIVE:
            return "active";
        case ADDRESS_SPACE_SWITCH_STUB_READY:
            return "ready";
        case ADDRESS_SPACE_SWITCH_STUB_BLOCKED:
            return "blocked";
        default:
            return "unknown";
    }
}

int address_space_switch_stub(struct address_space *space)
{
    if (!space)
    {
        return ADDRESS_SPACE_SWITCH_STUB_BLOCKED;
    }

    if (space->switch_status == ADDRESS_SPACE_SWITCH_ACTIVE)
    {
        space->switch_stub_status = ADDRESS_SPACE_SWITCH_STUB_ACTIVE;
        return space->switch_stub_status;
    }

    if (!space->switch_ready ||
        !space->target_root_table ||
        !space->validation_ready ||
        space->validation_errors)
    {
        space->switch_stub_status = ADDRESS_SPACE_SWITCH_STUB_BLOCKED;
        return space->switch_stub_status;
    }

    /*
     * The low-level TTBR0 switch primitive exists, but this milestone only
     * validates that a future switch is allowed. It deliberately does not call
     * arm64_mmu_switch_ttbr0().
     */
    space->switch_stub_status = ADDRESS_SPACE_SWITCH_STUB_READY;
    return space->switch_stub_status;
}
