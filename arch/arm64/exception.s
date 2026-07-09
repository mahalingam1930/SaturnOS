.section .text

.align 11

.global exception_vector

exception_vector:
    /* EL1t */
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80

    /* EL1h */
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80

    /* Lower EL AArch64 */
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80

    /* Lower EL AArch32 */
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80
    b exception_default_handler
    .balign 0x80

exception_default_handler:
    bl exception_handler
1:
    b 1b
