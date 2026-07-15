#include "programs.h"
#include "executable.h"
#include "vfs.h"

static const unsigned int user_demo_code[] = {
    0xd2800028U,
    0xd2800020U,
    0xd2a00401U,
    0xd28002e2U,
    0xd4000001U,
    0xd2800048U,
    0xd28000e0U,
    0xd4000001U,
    0xd4200000U,
};

static const char user_demo_message[] = "hello from EL0 syscall\n";

int user_programs_init(void)
{
    static unsigned char image[sizeof(struct saturn_exec_header) +
                               sizeof(user_demo_code) +
                               sizeof(user_demo_message) - 1UL]
        __attribute__((aligned(4)));
    struct saturn_exec_header *header =
        (struct saturn_exec_header *)image;
    unsigned char *payload = image + sizeof(*header);
    unsigned long payload_size = sizeof(user_demo_code) +
                                 sizeof(user_demo_message) - 1UL;

    header->magic = SATURN_EXEC_MAGIC;
    header->version = SATURN_EXEC_VERSION;
    header->header_size = sizeof(*header);
    header->code_size = sizeof(user_demo_code);
    header->data_size = sizeof(user_demo_message) - 1UL;
    header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_demo_code); i++)
    {
        payload[i] = ((const unsigned char *)user_demo_code)[i];
    }
    for (unsigned long i = 0; i < sizeof(user_demo_message) - 1UL; i++)
    {
        payload[sizeof(user_demo_code) + i] = user_demo_message[i];
    }
    header->payload_checksum = saturn_exec_checksum(payload, payload_size);

    if (!vfs_mkdir("/bin") ||
        !vfs_mkdir("/share") ||
        !vfs_create(USER_DEMO_IMAGE_PATH, image, sizeof(image)) ||
        !vfs_create(USER_DEMO_DATA_PATH,
                    user_demo_message,
                    sizeof(user_demo_message) - 1UL))
    {
        return 0;
    }

    vfs_create("/disk/bin/user-demo.sx", image, sizeof(image));
    return 1;
}
