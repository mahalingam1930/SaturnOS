CROSS = aarch64-elf-

CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra \
    -Ikernel/include \
    -Ikernel/console \
    -Idrivers/uart \
    -Iarch/arm64 \
	-Ikernel/panic \
	-Ikernel/exception

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

$(BUILD)/exception_asm.o: arch/arm64/exception.s | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/timer.o: arch/arm64/timer.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/panic.o: kernel/panic/panic.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/decoder.o: kernel/exception/decoder.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/saturnos.elf: \
    $(BUILD)/boot.o \
    $(BUILD)/kernel.o \
    $(BUILD)/console.o \
    $(BUILD)/kprintf.o \
    $(BUILD)/uart.o \
    $(BUILD)/exception.o \
    $(BUILD)/exception_asm.o \
	$(BUILD)/timer.o \
	$(BUILD)/panic.o \
	$(BUILD)/decoder.o
	$(LD) -T boot/linker.ld -o $@ $^

clean:
	rm -rf $(BUILD)

.PHONY: all clean
