#ifndef EXECUTABLE_H
#define EXECUTABLE_H

#define SATURN_EXEC_MAGIC 0x53415458U
#define SATURN_EXEC_VERSION 1U

struct saturn_exec_header
{
    unsigned int magic;
    unsigned short version;
    unsigned short header_size;
    unsigned int code_size;
    unsigned int data_size;
    unsigned int entry_offset;
    unsigned int payload_checksum;
};

struct saturn_exec_image
{
    const void *code;
    const void *data;
    unsigned long code_size;
    unsigned long data_size;
    unsigned long entry_offset;
};

unsigned int saturn_exec_checksum(const void *data, unsigned long size);
int saturn_exec_parse(const void *file,
                      unsigned long file_size,
                      struct saturn_exec_image *image);

#endif
