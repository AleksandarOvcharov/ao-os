# AO OS - Aleksandar Ovcharov's Operating System

A 64-bit x86-64 operating system written in C and Assembly with a custom bootloader and terminal-based interface.

## Features

- **Custom 2-Stage Bootloader**: No GRUB dependency, boots directly into 64-bit long mode with E820 memory detection
- **64-bit Long Mode**: Full x86-64 kernel with proper GDT, TSS, and 4-level paging
- **Memory Management**: E820 memory map, bitmap PMM (up to 4 GB), VMM with 4-level page tables
- **Process Management**: Process creation, round-robin scheduler, preemptive context switching
- **User Mode (Ring 3)**: User programs run in ring 3 with kernel/user separation via GDT and page tables
- **CPU Exception Handling**: All 22 CPU exceptions (ISR 0-21) with blue-screen register dump
- **Enhanced CPUID**: Brand string, feature flags (SSE, AVX, NX...), family/model/stepping
- **VGA Text Mode**: 80x25 color text display with 1000-line scrollback (PageUp/PageDown)
- **Interrupt-Driven Keyboard**: PS/2 keyboard via IRQ1 with ring buffer (no polling)
- **FAT12 Filesystem**: Full read/write with subdirectory support
- **AOB Executable Format**: Custom binary format for user programs with syscall API
- **40+ Shell Commands**: File operations, directory navigation, text editor, system tools, process management (`ps`, `kill`)
- **Interactive Shell**: Command history, colored prompts, working directory display
- Serial console output for debugging (COM1)

## Requirements

- NASM (Netwide Assembler)
- GCC cross-compiler (`x86_64-elf-gcc`)
- GNU Make
- QEMU (for testing)

## Building and Running

### Prerequisites

- `x86_64-elf-gcc` cross-compiler
- `nasm` assembler
- `qemu-system-x86_64` for testing

### Build Commands

```bash
make          # Build the OS
make run      # Run in QEMU (with serial console output)
make run-disk # Run with a second FAT12 floppy disk
make clean    # Clean build files
```

### Debugging

The kernel outputs debug logs to both VGA display and serial console (COM1). When running with `make run`, kernel logs will appear in your terminal via QEMU's serial output. CPU exceptions are also logged to serial with register values.

## Running

### With QEMU
```bash
make run
```

### With VirtualBox or other VM
Mount the `ao-os.iso` file as a disk image and boot from it.

## Cleaning

```bash
make clean
```

## Project Structure

```
ao-os/
├── bootloader/        # Custom 2-stage bootloader
│   ├── stage1.asm     # Stage 1 MBR (512 bytes)
│   └── stage2.asm     # Stage 2: boot menu, A20, 64-bit mode transition
├── boot/              # Kernel entry and low-level assembly
│   ├── boot.asm       # Kernel entry point (_start)
│   ├── interrupt.asm  # ISR 0-21 exception handlers, IRQ0/1, syscall handler
│   └── gdt.asm        # GDT (code/data/TSS segments) and TSS
├── kernel/            # Kernel source code
│   ├── kernel.c       # Kernel main and initialization
│   ├── shell.c        # Shell main loop
│   ├── idt.c          # IDT setup and PIC remapping
│   ├── gdt.c          # GDT/TSS runtime initialization
│   ├── exception.c    # CPU exception handler (blue screen + register dump)
│   ├── panic.c        # Kernel panic (red screen)
│   ├── cpu.c          # CPUID detection
│   ├── syscall.c      # Syscall dispatcher
│   ├── drivers/       # Device drivers
│   │   ├── vga.c      # VGA text mode driver with scrollback
│   │   ├── keyboard.c # PS/2 keyboard (IRQ1 interrupt-driven)
│   │   ├── timer.c    # PIT timer driver (100 Hz)
│   │   ├── serial.c   # COM1 serial console
│   │   ├── ata.c      # ATA PIO disk driver
│   │   └── rtc.c      # Real-time clock
│   ├── fs/            # Filesystem
│   │   ├── fs.c       # Filesystem abstraction layer
│   │   ├── fat12.c    # FAT12 with subdirectory support
│   │   └── ramfs.c    # RAM filesystem fallback
│   ├── memory/        # Memory management
│   │   ├── pmm.c      # Physical page frame allocator (bitmap, E820)
│   │   ├── vmm.c      # Virtual memory manager (4-level page tables)
│   │   └── memory.c   # Kernel heap allocator (kmalloc/kfree)
│   ├── shell/         # Shell commands
│   │   └── commands.c # 40+ built-in commands
│   ├── editor/        # Text editor
│   │   └── editor.c   # In-OS text editor with status bar
│   ├── process/       # Process management
│   │   └── process.c  # Process creation, scheduler, context switching
│   ├── aob/           # AOB executable loader
│   │   └── aob.c      # AOB format parser and executor
│   ├── installer/     # OS installer
│   │   └── installer.c
│   ├── system/        # System utilities
│   │   └── system.c   # Reboot/shutdown
│   └── lib/           # Library functions
│       └── string.c   # String utilities
├── include/           # Header files
├── documentation/     # Documentation
├── linker.ld          # Linker script (kernel at 1MB)
└── Makefile           # Build system
```

## Available Commands

- `help` - Display available commands
- `clear` - Clear the screen
- `echo <text>` - Print text to the screen
- `about` - Display OS information with colors
- `kernel -v` or `kernel --version` - Display kernel version and build information
- `sysinfo` - Display system information (CPU, RAM, uptime)
- `diskinfo` - Display disk controller information
- `sconsole --status` - Display serial console status
- `ls` - List files in RAM filesystem
- `cat <filename>` - Display file contents
- `edit <filename>` - Edit file with simple text editor (ESC to save)
- `write <filename> <content>` - Create or modify a file
- `rm <filename>` - Remove a file
- `touch <filename>` - Create an empty file
- `mem` - Display memory usage statistics
- `uptime` - Display system uptime
- `color <fg>` - Change text color (e.g., `color light_green`)
- `reboot` - Reboot the system
- `shutdown` - Shutdown the system

### Available Colors

black, blue, green, cyan, red, magenta, brown, light_grey, dark_grey, 
light_blue, light_green, light_cyan, light_red, light_magenta, yellow, white

## Architecture

```
bootloader --> kernel --> drivers --> fs --> memory --> process --> user programs
     |            |          |         |       |           |            |
  stage1/2    kernel.c    vga.c    fat12.c  pmm.c    scheduler     AOB format
              gdt/idt    keyboard   ramfs   vmm.c   context switch  syscalls
                          timer             heap    ring 3 support  int 0x80
```

### Process Model

AO OS implements a preemptive multitasking kernel with ring 0/ring 3 separation:

- **Kernel process (PID 0)** runs the shell in ring 0
- **User processes (PID 1+)** execute AOB binaries in ring 3 (user mode)
- **Round-robin scheduler** with 50ms time quantum, driven by PIT timer at 100Hz
- **Context switching** saves/restores all 15 GPRs + CPU interrupt frame (RIP, CS, RFLAGS, RSP, SS)
- **TSS** provides RSP0 for automatic ring 3 -> ring 0 stack switching on interrupts
- **Syscall interface** via `int 0x80` allows user programs to request kernel services

## Limitations

- Single user process at a time (shared virtual address space at 0x200000)
- No per-process page tables (all processes share the kernel page table)
- FAT12 filesystem without caching or permissions
- No networking support
- AOB executable format without dynamic linking or arguments passing to ring 3
- No inter-process communication (IPC)
- No signals or asynchronous event delivery

## Roadmap

- [ ] Per-process page tables (memory isolation between processes)
- [ ] True concurrent multitasking (multiple user processes with separate address spaces)
- [ ] Improved executable format (ELF support, dynamic linking)
- [ ] FAT12 write optimizations and sector caching
- [ ] Graphics mode (VESA framebuffer)
- [ ] Networking stack (NE2000/virtio-net)
- [ ] IPC mechanisms (pipes, shared memory)
- [ ] Virtual filesystem (VFS) with mount points

## Documentation

For detailed documentation, see the `documentation/` folder:
- **BUILD.md** - Detailed build instructions and toolchain setup
- **FEATURES.md** - Comprehensive feature list and roadmap
- **CHANGELOG.md** - Version history and changes
- **CONTRIBUTING.md** - Guidelines for contributing

## License

MIT License - Feel free to use and modify.

## Author

Aleksandar Ovcharov
