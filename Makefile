CROSS = aarch64-elf-

CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra \
	-Ikernel/include \
	-Ikernel/console \
	-Idrivers/uart

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

$(BUILD)/uart.o: drivers/uart/uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/saturnos.elf: \
	$(BUILD)/boot.o \
	$(BUILD)/kernel.o \
	$(BUILD)/console.o \
	$(BUILD)/uart.o
	$(LD) -T boot/linker.ld -o $@ $^

clean:
	rm -rf $(BUILD)

.PHONY: all clean