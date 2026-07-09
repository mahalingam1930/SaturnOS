#!/bin/bash

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -serial mon:stdio \
    -display cocoa,show-cursor=on \
    -kernel build/saturnos.elf
