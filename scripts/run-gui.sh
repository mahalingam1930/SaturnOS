#!/bin/bash

if [ ! -f saturnos.img ]; then
    truncate -s 4M saturnos.img
fi

qemu-system-aarch64 \
    -M virt,virtio-mmio-transports=32 \
    -cpu cortex-a57 \
    -serial mon:stdio \
    -device ramfb \
    -device virtio-keyboard-device,bus=virtio-mmio-bus.0 \
    -drive if=none,file=saturnos.img,format=raw,id=saturn-disk \
    -device virtio-blk-device,drive=saturn-disk \
    -display cocoa,show-cursor=on \
    -kernel build/saturnos.elf
