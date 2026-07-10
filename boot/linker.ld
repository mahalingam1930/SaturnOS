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

    .text :
    {
        *(.text*)
    } :text

    .rodata :
    {
        *(.rodata*)
    } :text

    .data :
    {
        *(.data*)
    } :data

    .bss :
    {
        *(.bss*)
        *(COMMON)
    } :data

    . = ALIGN(4096);
    _kernel_end = .;
}
