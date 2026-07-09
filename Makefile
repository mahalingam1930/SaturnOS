CROSS = aarch64-elf-

CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra \
    -Ikernel/include \
    -Ikernel/console \
    -Idrivers/uart \
    -Iarch/arm64

BUILD = build

all: $(BUILD)/saturnos.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.S | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/kernel.o: kernel/src/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/console.o: kernel/console/console.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kprintf.o: kernel/console/kprintf.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: drivers/uart/uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exception.o: arch/arm64/exception.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exception_asm.o: arch/arm64/exception.S | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/saturnos.elf: \
    $(BUILD)/boot.o \
    $(BUILD)/kernel.o \
    $(BUILD)/console.o \
    $(BUILD)/kprintf.o \
    $(BUILD)/uart.o \
    $(BUILD)/exception.o \
    $(BUILD)/exception_asm.o
	$(LD) -T boot/linker.ld -o $@ $^

clean:
	rm -rf $(BUILD)

.PHONY: all clean