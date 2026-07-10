CROSS = aarch64-elf-

CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mgeneral-regs-only -MMD -MP \
    -Ikernel/include \
    -Ikernel/console \
    -Idrivers/uart \
    -Iarch/arm64 \
	-Ikernel/panic \
	-Ikernel/exception \
	-Ikernel/sched \
	-Ikernel/demo \
	-Ikernel/input \
	-Ikernel/shell \
	-Ikernel/memory \
	-Idrivers/video \
	-Idrivers/keyboard

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

$(BUILD)/keyboard.o: drivers/keyboard/keyboard.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/framebuffer.o: drivers/video/framebuffer.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exception.o: arch/arm64/exception.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/exception_asm.o: arch/arm64/exception.s | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/context_asm.o: arch/arm64/context.S | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/timer.o: arch/arm64/timer.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/irq.o: arch/arm64/irq.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/scheduler.o: kernel/sched/scheduler.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/thread_demo.o: kernel/demo/thread_demo.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keyboard_input.o: kernel/input/keyboard_input.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: kernel/shell/shell.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: kernel/memory/pmm.c | $(BUILD)
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
	$(BUILD)/keyboard.o \
	$(BUILD)/framebuffer.o \
    $(BUILD)/exception.o \
    $(BUILD)/exception_asm.o \
	$(BUILD)/context_asm.o \
	$(BUILD)/timer.o \
	$(BUILD)/irq.o \
	$(BUILD)/scheduler.o \
	$(BUILD)/thread_demo.o \
	$(BUILD)/keyboard_input.o \
	$(BUILD)/shell.o \
	$(BUILD)/pmm.o \
	$(BUILD)/panic.o \
	$(BUILD)/decoder.o
	$(LD) -T boot/linker.ld -o $@ $^

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.d

.PHONY: all clean
