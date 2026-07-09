.section .text
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    bl kernel_main

1:
    b 1b

.section .bss
.align 12

stack:
    .skip 16384          // 16 KB stack

stack_top: