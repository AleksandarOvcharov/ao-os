# AO OS - Aleksandar Ovcharov's Operating System

A basic x86 operating system written in C and Assembly with a terminal-based interface.

## Features

- Custom bootloader (Multiboot-compliant)
- 32-bit protected mode kernel
- VGA text mode display driver with **color support**
- **Blinking cursor** that shows where you're typing
- PS/2 keyboard driver with shift key support
- Interactive terminal/shell interface with colored prompts
- Built-in commands: help, clear, echo, about, color, reboot, shutdown
- Modular code structure for easy expansion
- Serial console output for debugging

## Requirements

- NASM (Netwide Assembler)
- GCC cross-compiler for i686-elf
- GNU Make
- GRUB (for creating bootable ISO)
- xorriso (for ISO creation)
- QEMU (for testing)

## Building and Running

### Prerequisites

- `i686-elf-gcc` cross-compiler
- `nasm` assembler
- `grub-mkrescue` for creating bootable ISO
- `qemu-system-i386` for testing

### Build Commands

```bash
make          # Build the OS
make run      # Run in QEMU (with serial console output)
make clean    # Clean build files
```

### Debugging

The kernel outputs debug logs to both VGA display and serial console (COM1). When running with `make run`, kernel logs will appear in your terminal via QEMU's serial output. This is very useful for debugging kernel initialization and tracking system events.

## Running

### With QEMU
```bash
make run
```

### With VirtualBox or other VM
Mount the `ao-os.iso` file as a CD-ROM and boot from it.

## Cleaning

```bash
make clean
```

## Project Structure

```
ao-os/
├── boot/              # Bootloader code
│   └── boot.asm       # Multiboot bootloader
├── kernel/            # Kernel source code
│   ├── kernel.c       # Kernel entry point
│   ├── shell.c        # Shell main loop
│   ├── drivers/       # Device drivers
│   │   ├── vga.c      # VGA text mode driver
│   │   └── keyboard.c # PS/2 keyboard driver
│   ├── lib/           # Library functions
│   │   └── string.c   # String utilities
│   ├── shell/         # Shell commands
│   │   └── commands.c # Command implementations
│   └── system/        # System utilities
│       └── system.c   # Reboot/shutdown functions
├── include/           # Header files
│   ├── vga.h
│   ├── keyboard.h
│   ├── string.h
│   ├── shell.h
│   ├── commands.h
│   ├── system.h
│   └── io.h
├── documentation/     # Documentation files
│   ├── BUILD.md
│   ├── FEATURES.md
│   ├── CHANGELOG.md
│   └── CONTRIBUTING.md
├── build/             # Build output (generated)
├── iso/               # ISO creation directory (generated)
├── linker.ld          # Linker script
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
- `mem` - Display memory usage statistics
- `uptime` - Display system uptime
- `color <fg>` - Change text color (e.g., `color light_green`)
- `reboot` - Reboot the system
- `shutdown` - Shutdown the system

### Available Colors

black, blue, green, cyan, red, magenta, brown, light_grey, dark_grey, 
light_blue, light_green, light_cyan, light_red, light_magenta, yellow, white

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
