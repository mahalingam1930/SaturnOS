#!/bin/bash

qemu-system-aarch64 \
    -M virt,virtio-mmio-transports=32 \
    -cpu cortex-a57 \
    -serial mon:stdio \
    -device ramfb \
    -device virtio-keyboard-device,bus=virtio-mmio-bus.0 \
    -display cocoa,show-cursor=on \
    -kernel build/saturnos.elf
