ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* r-x */
    data PT_LOAD FLAGS(6);   /* rw- */
}

SECTIONS
{
    . = 0x40080000;
    _kernel_start = .;

    _text_start = .;
    .text :
    {
        *(.text*)
    } :text
    _text_end = .;

    . = ALIGN(4096);
    _rodata_start = .;
    .rodata :
    {
        *(.rodata*)
    } :text
    _rodata_end = .;

    . = ALIGN(4096);
    _data_start = .;
    .data :
    {
        *(.data*)
    } :data
    _data_end = .;

    . = ALIGN(4096);
    _bss_start = .;
    .bss :
    {
        *(.bss*)
        *(COMMON)
    } :data
    _bss_end = .;

    . = ALIGN(4096);
    _kernel_end = .;
}
