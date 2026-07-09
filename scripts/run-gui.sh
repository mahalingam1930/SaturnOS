#!/bin/bash

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -serial vc:1024x768 \
    -monitor stdio \
    -display cocoa,show-cursor=on \
    -kernel build/saturnos.elf
