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
static const unsigned int user_fault_code[] = {
    0xd4200000U,
};
static const unsigned int user_file_code[] = {
    0xd2800088U, 0xd2a00400U, 0xd2800221U, 0xd4000001U,
    0xaa0003e3U, 0xd28000a8U, 0xaa0303e0U, 0xd2a00401U,
    0x91010021U, 0xd2800302U, 0xd4000001U, 0xaa0003e2U,
    0xd2800028U, 0xd2800020U, 0xd4000001U, 0xd28000c8U,
    0xaa0303e0U, 0xd4000001U, 0xd2800048U, 0xd2800000U,
    0xd4000001U, 0xd4200000U,
};
static const char user_file_path[] = "/disk/syscall.txt";
static const char user_file_message[] = "hello from file syscall\n";
static const unsigned int user_file_write_code[] = {
    0xd28000e8U, 0xd2a00400U, 0xd2800281U, 0xd4000001U,
    0xaa0003e3U, 0xd2800028U, 0xaa0303e0U, 0xd2a00401U,
    0x91010021U, 0xd2800222U, 0xd4000001U, 0xd28000c8U,
    0xaa0303e0U, 0xd4000001U, 0xd2800048U, 0xd2800000U,
    0xd4000001U, 0xd4200000U,
};
static const char user_file_write_path[] = "/disk/user-write.txt";
static const char user_file_write_message[] = "written from EL0\n";
static const unsigned int user_file_seek_code[] = {
    0xd2800088U, 0xd2a00400U, 0xd2800221U, 0xd4000001U,
    0xaa0003e3U, 0xd2800108U, 0xaa0303e0U, 0xd28000c1U,
    0xd4000001U, 0xd28000a8U, 0xaa0303e0U, 0xd2a00401U,
    0x91010021U, 0xd2800242U, 0xd4000001U, 0xaa0003e2U,
    0xd2800028U, 0xd2800020U, 0xd4000001U, 0xd28000c8U,
    0xaa0303e0U, 0xd4000001U, 0xd2800048U, 0xd2800000U,
    0xd4000001U, 0xd4200000U,
};
static const unsigned int user_args_code[] = {
    0xf100081fU, 0x54000263U, 0xaa0103e3U, 0xaa0203e4U,
    0xf9400061U, 0xf9400082U, 0xd2800028U, 0xd2800020U,
    0xd4000001U, 0xd2800028U, 0xd2800020U, 0xd2a00401U,
    0x9101e021U, 0xd2800022U, 0xd4000001U, 0xf9400461U,
    0xf9400482U, 0xd2800028U, 0xd2800020U, 0xd4000001U,
    0xd2800048U, 0xd2800000U, 0xd4000001U, 0xd4200000U,
};
static const unsigned int user_spawn_code[] = {
    0xd28001c8U, 0xd4000001U, 0xd28001e8U, 0xd4000001U,
    0xd28001a8U, 0xd4000001U, 0xd2800148U, 0xd2a00400U,
    0xd2800221U, 0xd2a00402U,
    0x91008042U, 0xd2800143U, 0xd4000001U, 0xd2800188U,
    0xd2800280U, 0xd4000001U, 0xd28001a8U, 0xd4000001U,
    0xd2800128U, 0xd2800000U, 0xd2a00401U, 0x9100c021U,
    0xd2800002U, 0xd4000001U, 0xf100041fU, 0x540000e1U,
    0xd2800028U,
    0xd2800020U, 0xd2a00401U, 0x91014021U, 0xd2800102U,
    0xd4000001U, 0xd2800048U, 0xd2800000U, 0xd4000001U,
    0xd4200000U,
};
static const char user_spawn_path[] = "/bin/user-args.sx";
static const char user_spawn_arguments[] = "child args";
static const char user_spawn_wait_message[] = "wait ok\n";

int user_programs_init(void)
{
    static unsigned char image[sizeof(struct saturn_exec_header) +
                               sizeof(user_demo_code) +
                               sizeof(user_demo_message) - 1UL]
        __attribute__((aligned(4)));
    static unsigned char fault_image[sizeof(struct saturn_exec_header) +
                                     sizeof(user_fault_code)]
        __attribute__((aligned(4)));
    static unsigned char file_image[sizeof(struct saturn_exec_header) +
                                    sizeof(user_file_code) + 128UL]
        __attribute__((aligned(4)));
    static unsigned char file_write_image[
        sizeof(struct saturn_exec_header) +
        sizeof(user_file_write_code) + 128UL]
        __attribute__((aligned(4)));
    static unsigned char file_seek_image[
        sizeof(struct saturn_exec_header) +
        sizeof(user_file_seek_code) + 128UL]
        __attribute__((aligned(4)));
    static unsigned char args_image[sizeof(struct saturn_exec_header) +
                                    sizeof(user_args_code) + 128UL]
        __attribute__((aligned(4)));
    static unsigned char wait_image[sizeof(struct saturn_exec_header) +
                                    sizeof(user_spawn_code) + 128UL]
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

    struct saturn_exec_header *fault_header =
        (struct saturn_exec_header *)fault_image;
    unsigned char *fault_payload = fault_image + sizeof(*fault_header);
    fault_header->magic = SATURN_EXEC_MAGIC;
    fault_header->version = SATURN_EXEC_VERSION;
    fault_header->header_size = sizeof(*fault_header);
    fault_header->code_size = sizeof(user_fault_code);
    fault_header->data_size = 0;
    fault_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_fault_code); i++)
    {
        fault_payload[i] = ((const unsigned char *)user_fault_code)[i];
    }
    fault_header->payload_checksum =
        saturn_exec_checksum(fault_payload, sizeof(user_fault_code));

    struct saturn_exec_header *file_header =
        (struct saturn_exec_header *)file_image;
    unsigned char *file_payload = file_image + sizeof(*file_header);
    unsigned char *file_data = file_payload + sizeof(user_file_code);
    file_header->magic = SATURN_EXEC_MAGIC;
    file_header->version = SATURN_EXEC_VERSION;
    file_header->header_size = sizeof(*file_header);
    file_header->code_size = sizeof(user_file_code);
    file_header->data_size = 128;
    file_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_file_code); i++)
    {
        file_payload[i] = ((const unsigned char *)user_file_code)[i];
    }
    for (unsigned long i = 0; i < 128; i++)
    {
        file_data[i] = 0;
    }
    for (unsigned long i = 0; i < sizeof(user_file_path) - 1UL; i++)
    {
        file_data[i] = user_file_path[i];
    }
    file_header->payload_checksum =
        saturn_exec_checksum(file_payload,
                             sizeof(user_file_code) + 128UL);

    struct saturn_exec_header *file_write_header =
        (struct saturn_exec_header *)file_write_image;
    unsigned char *file_write_payload =
        file_write_image + sizeof(*file_write_header);
    unsigned char *file_write_data =
        file_write_payload + sizeof(user_file_write_code);
    file_write_header->magic = SATURN_EXEC_MAGIC;
    file_write_header->version = SATURN_EXEC_VERSION;
    file_write_header->header_size = sizeof(*file_write_header);
    file_write_header->code_size = sizeof(user_file_write_code);
    file_write_header->data_size = 128;
    file_write_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_file_write_code); i++)
    {
        file_write_payload[i] =
            ((const unsigned char *)user_file_write_code)[i];
    }
    for (unsigned long i = 0; i < 128; i++)
    {
        file_write_data[i] = 0;
    }
    for (unsigned long i = 0; i < sizeof(user_file_write_path) - 1UL; i++)
    {
        file_write_data[i] = user_file_write_path[i];
    }
    for (unsigned long i = 0; i < sizeof(user_file_write_message) - 1UL; i++)
    {
        file_write_data[64 + i] = user_file_write_message[i];
    }
    file_write_header->payload_checksum =
        saturn_exec_checksum(file_write_payload,
                             sizeof(user_file_write_code) + 128UL);

    struct saturn_exec_header *file_seek_header =
        (struct saturn_exec_header *)file_seek_image;
    unsigned char *file_seek_payload =
        file_seek_image + sizeof(*file_seek_header);
    unsigned char *file_seek_data =
        file_seek_payload + sizeof(user_file_seek_code);
    file_seek_header->magic = SATURN_EXEC_MAGIC;
    file_seek_header->version = SATURN_EXEC_VERSION;
    file_seek_header->header_size = sizeof(*file_seek_header);
    file_seek_header->code_size = sizeof(user_file_seek_code);
    file_seek_header->data_size = 128;
    file_seek_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_file_seek_code); i++)
    {
        file_seek_payload[i] =
            ((const unsigned char *)user_file_seek_code)[i];
    }
    for (unsigned long i = 0; i < 128; i++)
    {
        file_seek_data[i] = 0;
    }
    for (unsigned long i = 0; i < sizeof(user_file_path) - 1UL; i++)
    {
        file_seek_data[i] = user_file_path[i];
    }
    file_seek_header->payload_checksum =
        saturn_exec_checksum(file_seek_payload,
                             sizeof(user_file_seek_code) + 128UL);

    struct saturn_exec_header *args_header =
        (struct saturn_exec_header *)args_image;
    unsigned char *args_payload = args_image + sizeof(*args_header);
    args_header->magic = SATURN_EXEC_MAGIC;
    args_header->version = SATURN_EXEC_VERSION;
    args_header->header_size = sizeof(*args_header);
    args_header->code_size = sizeof(user_args_code);
    args_header->data_size = 128;
    args_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_args_code); i++)
    {
        args_payload[i] = ((const unsigned char *)user_args_code)[i];
    }
    for (unsigned long i = 0; i < 128; i++)
    {
        args_payload[sizeof(user_args_code) + i] = 0;
    }
    args_payload[sizeof(user_args_code) + 120] = ' ';
    args_header->payload_checksum =
        saturn_exec_checksum(args_payload, sizeof(user_args_code) + 128UL);

    struct saturn_exec_header *wait_header =
        (struct saturn_exec_header *)wait_image;
    unsigned char *wait_payload = wait_image + sizeof(*wait_header);
    wait_header->magic = SATURN_EXEC_MAGIC;
    wait_header->version = SATURN_EXEC_VERSION;
    wait_header->header_size = sizeof(*wait_header);
    wait_header->code_size = sizeof(user_spawn_code);
    wait_header->data_size = 128;
    wait_header->entry_offset = 0;
    for (unsigned long i = 0; i < sizeof(user_spawn_code); i++)
    {
        wait_payload[i] = ((const unsigned char *)user_spawn_code)[i];
    }
    for (unsigned long i = 0; i < 128; i++)
    {
        wait_payload[sizeof(user_spawn_code) + i] = 0;
    }
    for (unsigned long i = 0; i < sizeof(user_spawn_path) - 1UL; i++)
    {
        wait_payload[sizeof(user_spawn_code) + i] = user_spawn_path[i];
    }
    for (unsigned long i = 0; i < sizeof(user_spawn_arguments) - 1UL; i++)
    {
        wait_payload[sizeof(user_spawn_code) + 32UL + i] =
            user_spawn_arguments[i];
    }
    for (unsigned long i = 0;
         i < sizeof(user_spawn_wait_message) - 1UL;
         i++)
    {
        wait_payload[sizeof(user_spawn_code) + 80UL + i] =
            user_spawn_wait_message[i];
    }
    wait_header->payload_checksum = saturn_exec_checksum(
        wait_payload, sizeof(user_spawn_code) + 128UL);

    if (!vfs_mkdir("/bin") ||
        !vfs_mkdir("/share") ||
        !vfs_create(USER_DEMO_IMAGE_PATH, image, sizeof(image)) ||
        !vfs_create(USER_FAULT_IMAGE_PATH,
                    fault_image,
                    sizeof(fault_image)) ||
        !vfs_create(USER_FILE_IMAGE_PATH, file_image, sizeof(file_image)) ||
        !vfs_create(USER_FILE_WRITE_IMAGE_PATH,
                    file_write_image,
                    sizeof(file_write_image)) ||
        !vfs_create(USER_FILE_SEEK_IMAGE_PATH,
                    file_seek_image,
                    sizeof(file_seek_image)) ||
        !vfs_create(USER_ARGS_IMAGE_PATH, args_image, sizeof(args_image)) ||
        !vfs_create(USER_SPAWN_IMAGE_PATH, wait_image, sizeof(wait_image)) ||
        !vfs_create(USER_DEMO_DATA_PATH,
                    user_demo_message,
                    sizeof(user_demo_message) - 1UL))
    {
        return 0;
    }

    vfs_create("/disk/bin/user-demo.sx", image, sizeof(image));
    vfs_create("/disk/bin/user-fault.sx", fault_image, sizeof(fault_image));
    vfs_create("/disk/syscall.txt",
               user_file_message,
               sizeof(user_file_message) - 1UL);
    return 1;
}
