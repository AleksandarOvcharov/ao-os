# AO OS Makefile

AS = nasm
CC = x86_64-elf-gcc
LD = x86_64-elf-gcc

ASFLAGS_ELF = -f elf64
ASFLAGS_BIN = -f bin
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude \
          -mno-red-zone -mcmodel=kernel -mno-sse -mno-mmx -mno-sse2 \
          -MMD -MP
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib -lgcc

# Pull in auto-generated header dependencies
-include $(BUILD_DIR)/*.d

BUILD_DIR      = build
BOOT_DIR       = boot
BOOTLOADER_DIR = bootloader
KERNEL_DIR     = kernel

# Output — raw bootable disk image named .iso for familiarity
ISO_FILE = ao-os.iso

BOOT_OBJ = $(BUILD_DIR)/boot.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/gdt_asm.o
KERNEL_OBJS = \
    $(BUILD_DIR)/kernel.o \
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
    $(BUILD_DIR)/timer.o \
    $(BUILD_DIR)/cpu.o \
    $(BUILD_DIR)/klog.o \
    $(BUILD_DIR)/serial.o \
    $(BUILD_DIR)/ata.o \
    $(BUILD_DIR)/ramfs.o \
    $(BUILD_DIR)/fat12.o \
    $(BUILD_DIR)/fs.o \
    $(BUILD_DIR)/editor.o \
    $(BUILD_DIR)/installer.o \
    $(BUILD_DIR)/aob.o \
    $(BUILD_DIR)/syscall.o \
    $(BUILD_DIR)/rtc.o \
    $(BUILD_DIR)/exception.o \
    $(BUILD_DIR)/gdt.o \
    $(BUILD_DIR)/pmm.o \
    $(BUILD_DIR)/vmm.o \
    $(BUILD_DIR)/process.o

KERNEL_ELF  = $(BUILD_DIR)/ao-os.elf
KERNEL_BIN  = $(BUILD_DIR)/ao-os.bin
STAGE1_BIN  = $(BUILD_DIR)/stage1.bin
STAGE2_BIN  = $(BUILD_DIR)/stage2.bin
DISK_IMG    = $(BUILD_DIR)/ao-os.img

.PHONY: all clean run run-disk iso

all: iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Bootloader ─────────────────────────────────────────────────────────────

$(STAGE1_BIN): $(BOOTLOADER_DIR)/stage1.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(STAGE2_BIN): $(BOOTLOADER_DIR)/stage2.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

# ── Kernel ELF objects ─────────────────────────────────────────────────────

$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD_DIR)/interrupt.o: $(BOOT_DIR)/interrupt.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD_DIR)/gdt_asm.o: $(BOOT_DIR)/gdt.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS_ELF) $< -o $@

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

$(BUILD_DIR)/cpu.o: $(KERNEL_DIR)/cpu.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/klog.o: $(KERNEL_DIR)/klog.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: $(KERNEL_DIR)/drivers/serial.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ata.o: $(KERNEL_DIR)/drivers/ata.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ramfs.o: $(KERNEL_DIR)/fs/ramfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fat12.o: $(KERNEL_DIR)/fs/fat12.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs.o: $(KERNEL_DIR)/fs/fs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/editor.o: $(KERNEL_DIR)/editor/editor.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/installer.o: $(KERNEL_DIR)/installer/installer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aob.o: $(KERNEL_DIR)/aob/aob.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtc.o: $(KERNEL_DIR)/drivers/rtc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exception.o: $(KERNEL_DIR)/exception.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: $(KERNEL_DIR)/gdt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: $(KERNEL_DIR)/memory/pmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm.o: $(KERNEL_DIR)/memory/vmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process.o: $(KERNEL_DIR)/process/process.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Link kernel binary ──────────────────────────────────────────────────────

$(KERNEL_ELF): $(BOOT_OBJ) $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(KERNEL_BIN): $(KERNEL_ELF)
	x86_64-elf-objcopy -O binary $< $@

# ── Assemble disk image ────────────────────────────────────────────────────
# Layout:
#   sector 0        : stage1 (MBR)
#   sectors 1-4     : stage2
#   sectors 5+      : kernel flat binary

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) | $(BUILD_DIR)
	@KSECS=$$(( ($$(wc -c < $(KERNEL_BIN)) + 511) / 512 )); \
	 echo "Kernel: $$KSECS sectors ($$(wc -c < $(KERNEL_BIN)) bytes)"
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 seek=0 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=5 conv=notrunc 2>/dev/null

# ── Copy disk image to .iso (raw bootable image, .iso extension) ────────────
$(ISO_FILE): $(DISK_IMG)
	cp $< $@
	@echo "ISO built: $@"

iso: $(ISO_FILE)

# ── Run targets ────────────────────────────────────────────────────────────

run: iso
	qemu-system-x86_64 -drive file=$(ISO_FILE),format=raw,index=0,media=disk \
	                    -serial stdio -m 32

floppy.img:
	@echo "Creating floppy.img..."
	dd if=/dev/zero of=floppy.img bs=512 count=2880 2>/dev/null
	mkdosfs -F 12 floppy.img 2>/dev/null || mformat -i floppy.img -f 1440 :: 2>/dev/null || true

run-disk: iso floppy.img
	qemu-system-x86_64 -drive file=$(ISO_FILE),format=raw,index=0,media=disk \
	                    -drive file=floppy.img,format=raw,if=ide \
	                    -serial stdio -m 32

clean:
	rm -rf $(BUILD_DIR) $(ISO_FILE) floppy.img
