BUILD = build

CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 \
	-std=gnu11 \
	-ffreestanding \
	-O2 \
	-Wall \
	-Wextra \
	-fno-pie \
	-fno-pic \
	-fno-stack-protector \
	-fno-builtin \
	-fno-omit-frame-pointer \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-msoft-float

LDFLAGS = -m elf_i386 \
	-T linker.ld \
	-nostdlib

C_SOURCES = \
	kernel/kernel.c \
	kernel/cpu/gdt.c \
	kernel/cpu/idt.c \
	kernel/drivers/terminal.c \
	kernel/drivers/keyboard.c \
	kernel/drivers/ata.c \
	kernel/filesystem/fat32.c \
	kernel/drivers/pit.c \
	kernel/memory/memory.c \
	kernel/memory/paging.c \
	kernel/memory/heap.c \
	kernel/task/task.c \
	kernel/shell/shell.c

ASM_SOURCES = \
	boot/boot.asm \
	kernel/cpu/interrupts.asm \
	kernel/cpu/task_switch.asm

C_OBJECTS = $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SOURCES))

OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

all: iso

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(BUILD)/kernel.bin: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

check: $(BUILD)/kernel.bin
	grub-file --is-x86-multiboot $(BUILD)/kernel.bin

iso: check
	mkdir -p iso/boot
	cp $(BUILD)/kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o $(BUILD)/PineconeOS.iso iso

clean:
	rm -rf $(BUILD)
	rm -f iso/boot/kernel.bin

.PHONY: all iso clean check