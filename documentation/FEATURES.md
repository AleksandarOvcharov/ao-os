# AO OS Features

## Current Features (v0.2.0)

### Core System
- **Bootloader**: Multiboot-compliant bootloader compatible with GRUB
- **Kernel**: 32-bit protected mode kernel written in C and Assembly
- **Memory Management**: Basic stack setup and memory initialization

### Display
- **VGA Text Mode**: 80x25 character display
- **Color Support**: 16 foreground and 16 background colors
- **Scrolling**: Automatic screen scrolling when text reaches bottom
- **Special Characters**: Support for newline, tab, and backspace

### Input
- **Keyboard Driver**: PS/2 keyboard support
- **Shift Key**: Proper handling of shift key for uppercase and symbols
- **Scancode Translation**: ASCII conversion from keyboard scancodes

### Shell/Terminal
- **Interactive Shell**: Command-line interface with prompt
- **Command Parsing**: Parse commands and arguments
- **Colored Prompt**: Green prompt for better visibility
- **Error Handling**: Red error messages for invalid commands
- **Command Buffer**: 256-character command buffer

### Commands

#### Information Commands
- **help**: Display all available commands with descriptions
- **about**: Show OS information with colored output

#### Display Commands
- **clear**: Clear the screen
- **echo**: Print text to the screen
- **color**: Change terminal colors (16 foreground × 16 background = 256 combinations)

#### System Commands
- **reboot**: Restart the computer using keyboard controller reset
- **shutdown**: Power off the system using ACPI (works in QEMU and some VMs)

### Code Organization
- **Modular Structure**: Separated into logical components
  - Drivers (VGA, Keyboard)
  - Libraries (String utilities)
  - Shell (Command parsing and execution)
  - System (Hardware control)
- **Header Files**: Clean API separation
- **Build System**: Automated build with Makefile

## Planned Features

### Short Term
- [ ] Command history (up/down arrows)
- [ ] Tab completion
- [ ] More string utilities
- [ ] Memory information command
- [ ] CPU information detection

### Medium Term
- [ ] File system support (basic)
- [ ] Text editor
- [ ] Calculator command
- [ ] Timer/clock display
- [ ] Multiple virtual terminals

### Long Term
- [ ] Multitasking
- [ ] User mode vs kernel mode
- [ ] System calls
- [ ] Process management
- [ ] Network stack (basic)
- [ ] Graphics mode support

## Technical Specifications

- **Architecture**: x86 (32-bit)
- **Boot Protocol**: Multiboot 1
- **Display**: VGA Text Mode 3 (80x25)
- **Input**: PS/2 Keyboard (Port 0x60/0x64)
- **Language**: C (kernel) and Assembly (bootloader)
- **Compiler**: GCC cross-compiler (i686-elf-gcc)
- **Assembler**: NASM
- **Linker**: GNU LD (via GCC)
- **Bootloader**: GRUB 2

## Hardware Requirements

- **Minimum**:
  - x86-compatible CPU (i386 or later)
  - 32 MB RAM
  - VGA-compatible display
  - PS/2 keyboard
  
- **Recommended** (for virtual machines):
  - 64 MB RAM
  - QEMU, VirtualBox, or VMware
