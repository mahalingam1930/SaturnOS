#!/bin/bash

if [ ! -f saturnos.img ]; then
    truncate -s 4M saturnos.img
fi

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -nographic \
    -drive if=none,file=saturnos.img,format=raw,id=saturn-disk \
    -device virtio-blk-device,drive=saturn-disk \
    -kernel build/saturnos.elf
