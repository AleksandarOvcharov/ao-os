# AO OS Makefile

AS = nasm
CC = i686-elf-gcc
LD = i686-elf-gcc

ASFLAGS = -f elf32
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD_DIR = build
ISO_DIR = iso
BOOT_DIR = boot
KERNEL_DIR = kernel

BOOT_OBJ = $(BUILD_DIR)/boot.o $(BUILD_DIR)/interrupt.o
KERNEL_OBJS = $(BUILD_DIR)/kernel.o \
              $(BUILD_DIR)/vga.o \
              $(BUILD_DIR)/keyboard.o \
              $(BUILD_DIR)/string.o \
              $(BUILD_DIR)/shell.o \
              $(BUILD_DIR)/commands.o \
              $(BUILD_DIR)/system.o \
              $(BUILD_DIR)/version.o \
              $(BUILD_DIR)/panic.o \
              $(BUILD_DIR)/memory.o \
              $(BUILD_DIR)/idt.o \
              $(BUILD_DIR)/timer.o

KERNEL_BIN = $(BUILD_DIR)/ao-os.bin
ISO_FILE = ao-os.iso

.PHONY: all clean run iso

all: $(ISO_FILE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR):
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: $(BOOT_DIR)/interrupt.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: $(KERNEL_DIR)/drivers/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(KERNEL_DIR)/drivers/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: $(KERNEL_DIR)/lib/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: $(KERNEL_DIR)/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/commands.o: $(KERNEL_DIR)/shell/commands.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system.o: $(KERNEL_DIR)/system/system.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/version.o: $(KERNEL_DIR)/version.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: $(KERNEL_DIR)/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memory.o: $(KERNEL_DIR)/memory/memory.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: $(KERNEL_DIR)/drivers/timer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(ISO_FILE): $(KERNEL_BIN) | $(ISO_DIR)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/ao-os.bin
	echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "AO OS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/ao-os.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR) 2>&1 | grep -v "xorriso"

iso: $(ISO_FILE)

run: $(KERNEL_BIN)
	qemu-system-i386 -kernel $(KERNEL_BIN)

run-iso: $(ISO_FILE)
	qemu-system-i386 -cdrom $(ISO_FILE)

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_FILE)
