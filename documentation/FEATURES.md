# AO OS Features

## AO-OS Features (v2.0.0 - Nova)

### Core System
- **Custom 2-Stage Bootloader**: Replaces GRUB, full control over boot sequence
  - Stage 1 (MBR): Loads stage 2 from disk
  - Stage 2: Interactive boot menu, A20 line, 64-bit mode transition, kernel loading
- **Kernel**: 64-bit long mode kernel written in C and Assembly
- **Global Descriptor Table (GDT)**: 6-entry GDT in assembly
  - Null, kernel code (0x08), kernel data (0x10), TSS (0x18, 16-byte), user data (0x28), user code (0x30)
  - 64-bit long mode descriptors with proper flags
  - Runtime GDT reload with far return for CS and data segment refresh
- **Task State Segment (TSS)**:
  - TSS descriptor (0x18) in GDT, filled at runtime with actual TSS address
  - RSP0 updated on every context switch for ring 3 → ring 0 stack switching
  - IST1 with dedicated 4KB stack for Double Fault (prevents triple-fault)
- **Preemptive Multitasking**:
  - Round-robin scheduler with 50ms time quantum
  - Timer-driven (PIT at 100Hz) preemptive context switching
  - Full register save/restore (15 GPRs + iretq frame) in assembly
  - Up to 8 concurrent processes with per-process 8KB kernel stacks
  - Process states: UNUSED, READY, RUNNING, BLOCKED, TERMINATED
- **User Mode (Ring 3)**:
  - User programs execute in ring 3 with hardware-enforced privilege separation
  - Separate user code/data GDT segments with DPL=3
  - User memory: code at 0x200000, stack at 0x800000 (16KB)
  - TSS RSP0 ensures clean ring 3 → ring 0 transitions on interrupts
- **Process Management**:
  - Create, terminate, wait, and list processes
  - Kernel process (PID 0) runs the shell
  - User processes (PID 1+) run AOB binaries
  - `ps` and `kill` shell commands
- **Syscall Interface (int 0x80)**:
  - System call dispatcher with SYS_WRITE, SYS_EXIT, SYS_OPEN, SYS_READ, SYS_CLOSE
  - IDT gate at DPL=3 for user-mode access
  - File descriptor table for user program I/O
- **Version System**: Kernel version tracking with build date
- **Panic System**: Kernel panic handler for fatal errors with colored screen
- **CPU Exception Handling (ISR 0-21)**:
  - All 22 CPU exceptions handled: Division by Zero, Debug, NMI, Breakpoint, Overflow, Bound Range, Invalid Opcode, Device Not Available, Double Fault, Invalid TSS, Segment Not Present, Stack Fault, GPF, Page Fault, x87 FPE, Alignment Check, Machine Check, SIMD FPE, and more
  - Blue-screen crash display with full register dump (RAX-R15, CR2, RIP, RFLAGS, CS, SS, RSP)
  - Page Fault details: faulting address, read/write, present/not-present, user/kernel
  - Double Fault uses IST1 dedicated stack for reliability
  - Diagnostics logged to serial console
- **Kernel Logging**:
  - Log levels: INFO, WARN, ERROR, DEBUG
  - Colored output for different log levels
  - Functions: klog(), klog_info(), klog_warn(), klog_error(), klog_debug()
  - Useful for debugging and system monitoring
- **Interrupt Handling**:
  - Interrupt Descriptor Table (IDT) with 256 entries
  - PIC remapping for IRQ handling
  - 22 CPU exception handlers (ISR 0-21) with uniform stack frame
  - Timer interrupt (IRQ0) and keyboard interrupt (IRQ1) support
  - Syscall gate (INT 0x80) with DPL=3 for user programs
  - IST support for critical exception handlers
- **Timer System**:
  - PIT (Programmable Interval Timer) driver
  - System tick counter at 100 Hz
  - Uptime tracking
- **Memory Management**:
  - **E820 Memory Map**: BIOS INT 0x15 detection in bootloader, accurate physical RAM layout
  - **Physical Page Frame Allocator (PMM)**: Bitmap-based, manages all 4KB pages up to 4 GB
  - **Virtual Memory Manager (VMM)**: 4-level page tables (PML4/PDPT/PD/PT), map/unmap 4KB pages, 2MB huge page splitting
  - **Kernel Heap**: 1 MB heap placed after kernel image (`_kernel_end`), backed by PMM
  - Dynamic memory allocation (kmalloc/kfree)
  - Block-based allocator with automatic merging
  - Memory safety checks (double-free, invalid pointer detection)
- **Enhanced CPUID**:
  - CPU brand string (48-char), vendor, family/model/stepping
  - Feature detection: FPU, SSE, SSE2, SSE3, SSE4.1, SSE4.2, AVX, APIC, NX, Long Mode

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

### Executable Format
- **AOB (Aleksandar Ovcharov's Binary) Format**:
  - Custom executable format with 128-byte header
  - Magic number validation: `0x00424F41` ("AOB\0")
  - Program metadata: name, version, entry point, code size
  - Direct execution from shell by typing filename
  - Example: `hello.aob` runs the program
- **Raw Binary (.bin) Support**:
  - Execute raw x86 machine code directly
  - No header required - pure binary execution
  - Example: `hello.bin` runs the raw binary
- **AOB Loader**:
  - Validates magic number and version
  - Loads code into memory buffer
  - Executes at specified entry point
  - Returns control to kernel after execution
- **aobcompile Tool**:
  - Converts raw binary to AOB format
  - Adds proper header with metadata
  - Command-line interface: `aobcompile input.bin output.aob [name]`
  - Validates code size limits
  - Maximum executable size: 16KB
- **ao.h - User Program API**:
  - Kernel API for C programs
  - Terminal I/O functions: `ao_print()`, `ao_println()`, `ao_putchar()`
  - Color control: `ao_setcolor()`
  - String utilities: `ao_strlen()`, `ao_strcmp()`, `ao_strcpy()`
  - Memory utilities: `ao_memset()`, `ao_memcpy()`
  - Integer printing: `ao_printint()`

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
  - Timeout-protected wait functions (no infinite hangs)
  - Error detection and status checking
  - Graceful fallback when no disk is attached
- **RAM Filesystem (ramfs)**:
  - In-memory filesystem (data is lost on reboot)
  - Maximum of 16 files
  - Maximum file size: 512 bytes
  - Maximum filename length: 31 characters + null terminator
  - Supports basic operations: create, write, read, list, delete
  - Automatic null terminator for text files
  - Fallback when no disk filesystem detected

### Input
- **Keyboard Driver**: PS/2 keyboard with interrupt-driven input (IRQ1)
  - IRQ1 handler in assembly with proper register save/restore and PIC EOI
  - 256-byte ring buffer for non-blocking input
  - `hlt`-based wait loops for power-efficient key waiting
  - No busy-wait polling of port 0x60
- **Shift Key**: Proper handling of shift key for uppercase and symbols
- **Ctrl Key**: Ctrl+key combinations (e.g., Ctrl+S in editor)
- **Arrow Keys**: Detection of arrow keys for navigation
- **Extended Keys**: PageUp, PageDown for scrollback
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
- [ ] Tab completion
- [ ] Shell piping and redirection (`|`, `>`, `<`)
- [ ] Shell scripting / batch files

### Medium Term
- [ ] Multiple virtual terminals
- [ ] PCI bus enumeration
- [ ] DMA for disk I/O
- [ ] Per-process page tables (memory isolation)
- [ ] ELF executable support

### Long Term
- [ ] True concurrent user processes with separate address spaces
- [ ] IPC mechanisms (pipes, shared memory)
- [ ] Network stack (basic)
- [ ] Graphics mode support (VESA framebuffer)
- [ ] Virtual filesystem (VFS) with mount points

## Technical Specifications

- **Architecture**: x86-64 (64-bit long mode)
- **Boot Protocol**: Custom 2-stage bootloader (no GRUB)
- **Display**: VGA Text Mode 3 (80x25)
- **Input**: PS/2 Keyboard (IRQ1 interrupt-driven, Port 0x60/0x64)
- **Language**: C (kernel) and Assembly (bootloader, interrupts, GDT)
- **Compiler**: GCC cross-compiler (x86_64-elf-gcc)
- **Assembler**: NASM
- **Linker**: GNU LD (via GCC)
- **Bootloader**: Custom 2-stage (stage1 MBR + stage2 mode switching)

## Hardware Requirements

- **Minimum**:
  - x86-64 compatible CPU (64-bit long mode support)
  - 32 MB RAM
  - VGA-compatible display
  - PS/2 keyboard

- **Recommended** (for virtual machines):
  - 64 MB RAM
  - QEMU (qemu-system-x86_64), VirtualBox, or VMware
