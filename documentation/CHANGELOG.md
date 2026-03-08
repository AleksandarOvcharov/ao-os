# Changelog

All notable changes to AO OS will be documented in this file.

## [0.3.3] - 2026-03-08

### Added
- **Kernel logging system**
  - Log levels: INFO, WARN, ERROR, DEBUG
  - Colored output for each log level (green, yellow, red, cyan)
  - Helper functions: klog_info(), klog_warn(), klog_error(), klog_debug()
  - Used during kernel initialization for debugging
  - Outputs to both VGA and serial console
- **Serial console (COM1)**
  - COM1 serial port driver for debugging
  - Serial output integrated with klog system
  - Use `qemu-system-i386 -serial stdio` to see kernel logs in terminal
  - Very useful for kernel debugging
- **Sysinfo command**
  - Display CPU vendor using CPUID instruction
  - Show RAM size
  - Display system uptime
  - Show OS version

### Changed
- Version bumped to 0.3.3
- Kernel initialization now shows detailed boot messages with klog
- QEMU run commands now include `-serial stdio` for serial console output

## [0.3.2] - 2026-03-08

### Added
- **PIT timer driver**
  - Programmable Interval Timer (PIT) initialization
  - System tick counter at 100 Hz
  - Timer interrupt handling (IRQ0)
- **Interrupt Descriptor Table (IDT)**
  - IDT initialization and management
  - PIC remapping for proper IRQ handling
  - Interrupt handler stubs in assembly
- **Uptime command**
  - `uptime` command to display system uptime
  - Human-readable format (days, hours, minutes, seconds)
  - Real-time tick counting

### Changed
- Version bumped to 0.3.2
- Kernel now initializes IDT and timer on boot
- Interrupts enabled for timer functionality

## [0.3.1] - 2026-03-08

### Added
- **Command history**
  - Up/Down arrow key navigation through command history
  - Stores last 10 commands
  - Prevents duplicate consecutive commands in history
  - Arrow key detection in keyboard driver (KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT)

### Changed
- Version bumped to 0.3.1
- Enhanced keyboard driver to detect extended keys (arrow keys)

## [0.3.0] - 2026-03-08

### Added
- **Physical memory manager**
  - Heap allocator with 1 MB heap space
  - `kmalloc()` function for dynamic memory allocation
  - `kfree()` function for memory deallocation
  - Block-based memory management with merging of free blocks
  - Memory usage tracking (used/free memory)
- **Memory command**
  - `mem` command to display memory usage statistics
  - Shows total heap, used memory, and free memory
  - Human-readable format (bytes, KB, MB)
- **Memory safety**
  - Double-free detection with kernel panic
  - Invalid pointer detection
  - Automatic memory block merging

### Changed
- Version bumped to 0.3.0
- Kernel now initializes memory manager on boot

## [0.2.1] - 2026-03-08

### Added
- **Kernel panic system**
  - `panic()` function for fatal errors
  - Displays error message and halts CPU
  - Colored panic screen (red background with white/yellow text)
  - `panic_assert()` function for assertion failures
  - `PANIC()` and `ASSERT()` macros for easy use
- **Kernel version system**
  - New `version.h` header with version constants
  - `kernel --version` or `kernel -v` command to display kernel information
  - Version information includes: version number, codename, build date
  - Colored output for kernel version display
- **Blinking hardware cursor**
  - VGA hardware cursor that blinks automatically
  - Cursor follows typing in real-time
  - Updates on all terminal operations (typing, backspace, newline, clear)

### Changed
- Updated `help` command to include `kernel` command
- Version bumped to 0.2.1 (codename: Aurora)

## [0.2.0] - 2026-03-08

### Added
- **Color support** throughout the OS
  - VGA driver now supports 16 foreground and background colors
  - Colored welcome banner
  - Colored command prompt (green)
  - Colored error messages (red)
  - `color` command to change terminal colors dynamically
- **System commands**
  - `reboot` - Reboot the system using keyboard controller
  - `shutdown` - Shutdown the system using ACPI
- **Improved code organization**
  - Separated commands into `kernel/shell/commands.c`
  - Created system utilities in `kernel/system/system.c`
  - Added `io.h` for port I/O operations
  - Added `commands.h` and `system.h` headers
- Enhanced `about` command with colored output
- Enhanced `help` command with color information

### Changed
- Reorganized project structure for better maintainability
- Updated Makefile to support new file structure
- Improved shell prompt with color
- Version bumped to 0.2.0

### Technical Details
- Implemented `vga_entry_color()` as public API
- Created modular command system for easy expansion
- Added hardware I/O abstraction layer

## [0.1.0] - 2026-03-08

### Added
- Initial release
- Multiboot-compliant bootloader
- 32-bit protected mode kernel
- VGA text mode driver (80x25)
- PS/2 keyboard driver with shift key support
- Basic shell with command parsing
- Commands: help, clear, echo, about
- String utility functions
- Build system with Makefile
- Documentation (README.md, BUILD.md, CONTRIBUTING.md)
- Git configuration (.gitignore, .editorconfig)
- MIT License
