#include "console.h"

void kernel_main(void)
{
    console_init();

    console_write("================================\n");
    console_write("      Welcome to SaturnOS\n");
    console_write("           Version 0.1\n");
    console_write("================================\n");

    while (1)
    {
    }
}