#include "executable.h"
#include "mmu.h"

unsigned int saturn_exec_checksum(const void *data, unsigned long size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned int checksum = 0;

    for (unsigned long i = 0; i < size; i++)
    {
        checksum = (checksum << 5) ^ (checksum >> 2) ^ bytes[i];
    }
    return checksum;
}

int saturn_exec_parse(const void *file,
                      unsigned long file_size,
                      struct saturn_exec_image *image)
{
    const struct saturn_exec_header *header;
    const unsigned char *payload;
    unsigned long payload_size;

    if (!file || !image || file_size < sizeof(struct saturn_exec_header))
    {
        return 0;
    }

    header = (const struct saturn_exec_header *)file;
    if (header->magic != SATURN_EXEC_MAGIC ||
        header->version != SATURN_EXEC_VERSION ||
        header->header_size != sizeof(struct saturn_exec_header) ||
        header->code_size == 0 ||
        header->code_size > ARM64_PAGE_SIZE ||
        header->data_size > ARM64_PAGE_SIZE ||
        header->entry_offset >= header->code_size ||
        (header->entry_offset & 3U))
    {
        return 0;
    }

    payload_size = (unsigned long)header->code_size + header->data_size;
    if (payload_size > file_size - header->header_size ||
        header->header_size + payload_size != file_size)
    {
        return 0;
    }

    payload = (const unsigned char *)file + header->header_size;
    if (saturn_exec_checksum(payload, payload_size) !=
        header->payload_checksum)
    {
        return 0;
    }

    image->code = payload;
    image->data = payload + header->code_size;
    image->code_size = header->code_size;
    image->data_size = header->data_size;
    image->entry_offset = header->entry_offset;
    return 1;
}
