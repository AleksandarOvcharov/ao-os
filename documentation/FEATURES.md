# AO OS Features

## AO-OS Features (v0.9.0) - Aurora)

### Core System
- **Bootloader**: Multiboot-compliant bootloader compatible with GRUB
- **Kernel**: 32-bit protected mode kernel written in C and Assembly
- **Version System**: Kernel version tracking with build date
- **Panic System**: Kernel panic handler for fatal errors with colored screen
- **Kernel Logging**:
  - Log levels: INFO, WARN, ERROR, DEBUG
  - Colored output for different log levels
  - Functions: klog(), klog_info(), klog_warn(), klog_error(), klog_debug()
  - Useful for debugging and system monitoring
- **Interrupt Handling**:
  - Interrupt Descriptor Table (IDT) with 256 entries
  - PIC remapping for IRQ handling
  - Timer interrupt (IRQ0) support
- **Timer System**:
  - PIT (Programmable Interval Timer) driver
  - System tick counter at 100 Hz
  - Uptime tracking
- **Memory Management**: 
  - Physical memory manager with 1 MB heap
  - Dynamic memory allocation (kmalloc/kfree)
  - Block-based allocator with automatic merging
  - Memory safety checks (double-free, invalid pointer detection)

### Display
- **VGA Text Mode**: 80x25 character display
- **Color Support**: 16 foreground and 16 background colors
- **Hardware Cursor**: Blinking cursor that follows typing in real-time
- **Scrolling**: Automatic screen scrolling when text reaches bottom
- **Terminal Scrollback**: 
  - 1000 lines of history buffer
  - PageUp/PageDown to scroll through previous output
  - Automatic scroll to bottom when typing
  - Color preservation in history
- **Special Characters**: Support for newline, tab, and backspace
- **Serial Console**: COM1 serial port output for debugging (use with `qemu -serial stdio`)

### Storage
- **Filesystem Abstraction Layer**:
  - Generic filesystem interface (`fs_init()`, `fs_create()`, `fs_read()`, `fs_delete()`, `fs_list()`)
  - Decouples applications from filesystem implementations
  - Automatic filesystem detection
  - Clean architecture for multiple filesystem support
- **FAT12 Filesystem**:
  - Full read/write support for FAT12-formatted disks
  - **Complete subdirectory support** - create and navigate directory trees
  - Boot sector and BPB parsing
  - FAT table and cluster chain navigation
  - Directory cluster reading and writing
  - File listing, reading, and writing in any directory
  - Multi-cluster file support (files up to 4KB)
  - Cluster chain allocation and traversal
  - Persistent storage support
  - Create, delete, and update files/directories anywhere in filesystem
- **ATA PIO Driver**: Primary ATA controller support
  - Read/write sectors using PIO mode
  - LBA28 addressing (up to 128GB disks)
  - Error detection and status checking
  - Foundation for filesystem implementation
- **RAM Filesystem (ramfs)**:
  - In-memory filesystem (data is lost on reboot)
  - Maximum of 16 files
  - Maximum file size: 512 bytes
  - Maximum filename length: 31 characters + null terminator
  - Supports basic operations: create, write, read, list, delete
  - Automatic null terminator for text files
  - Fallback when no disk filesystem detected

### Input
- **Keyboard Driver**: PS/2 keyboard support
- **Shift Key**: Proper handling of shift key for uppercase and symbols
- **Arrow Keys**: Detection of arrow keys for navigation
- **Scancode Translation**: ASCII conversion from keyboard scancodes

### Shell/Terminal
- **Interactive Shell**: Command-line interface with prompt
- **Command Parsing**: Parse commands and arguments with flag support
- **Command History**: Navigate through last 10 commands with Up/Down arrows
- **Colored Prompt**: Green prompt for better visibility
- **Error Handling**: Red error messages for invalid commands
- **Command Buffer**: 256-character command buffer
- **Visual Feedback**: Blinking cursor shows typing position

### Commands

#### Information Commands
- **help**: Display all available commands with descriptions
- **about**: Show OS information with colored output
- **kernel**: Display kernel version information (supports -v and --version flags)
- **sysinfo**: Display system information (CPU vendor, RAM, uptime, OS version)
- **diskinfo**: Display ATA disk controller status and configuration
- **sconsole**: Display serial console status (use `sconsole --status`)
- **mem**: Display memory usage statistics (total, used, free)
- **uptime**: Display system uptime in human-readable format

#### Display Commands
- **clear**: Clear the screen
- **echo**: Print text to the screen
- **color**: Change text foreground color (16 colors available)

#### Filesystem Commands
- **ls**: List all files and directories in filesystem
  - Files shown with size in bytes
  - Directories shown with blue [DIR] marker
- **cat**: Display file contents (usage: `cat <filename>`)
- **edit**: Enhanced text editor (usage: `edit <filename>`)
  - Live status bar with cursor position (line:column)
  - Character counter (current/max buffer usage)
  - Save indicator - shows "SAVED" when file is saved
  - Ctrl+S to save without exiting
  - ESC to save and exit
  - Beautiful box-drawing UI with borders
  - Type text, Enter for newline, Backspace to delete
  - PageUp/PageDown to scroll through content
  - Real-time position tracking
  - 4KB buffer (supports multi-cluster files)
- **write**: Create or modify files (usage: `write <filename> <content>`)
- **rm**: Remove files (usage: `rm <filename>`)
- **touch**: Create empty files (usage: `touch <filename>`)
- **mkdir**: Create directory in current location (usage: `mkdir <dirname>`)
  - Works in root and subdirectories
  - Creates nested directory structures
- **rmdir**: Remove directory (usage: `rmdir <dirname>`)
  - Deletes directory and frees clusters
- **cd**: Navigate directory tree (usage: `cd <dirname>`)
  - `cd /` - Go to root directory
  - `cd ..` - Go to parent directory
  - `cd dirname` - Enter subdirectory
  - Full directory tree navigation
- **pwd**: Print working directory path

#### System Commands
- **install**: Install AO OS to disk (with confirmation prompt)
  - Detects ATA disk
  - Prepares disk (clears sectors)
  - Formats FAT12 filesystem
  - Installs kernel to disk
  - Installs bootloader to MBR
  - Interactive y/n confirmation
- **reboot**: Restart the computer using keyboard controller reset
- **shutdown**: Power off the system using ACPI (works in QEMU and some VMs)

### Code Organization
- **Modular Structure**: Separated into logical components
  - Drivers (VGA, Keyboard)
  - Libraries (String utilities)
  - Shell (Command parsing and execution)
  - System (Hardware control, Panic handler)
- **Header Files**: Clean API separation
- **Build System**: Automated build with Makefile
- **Error Handling**: Kernel panic system for fatal errors

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
