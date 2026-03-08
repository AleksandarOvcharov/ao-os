# Changelog

All notable changes to AO OS will be documented in this file.

## [0.7.0] - 2026-03-08

### Added
- **OS Installer System**
  - Minimal installer for installing AO OS to disk
  - Disk detection and validation
  - Disk preparation (clearing sectors)
  - FAT12 filesystem formatting
  - Kernel installation to disk
  - Bootloader installation
  - Interactive confirmation prompt
  - Colored status messages
  - Step-by-step installation process
  - `install` shell command

### Features
- **Installer Components**:
  - `installer_detect_disk()` - Detects ATA disk
  - `installer_prepare_disk()` - Clears first 10 sectors
  - `installer_format_filesystem()` - Creates FAT12 filesystem structure
  - `installer_install_kernel()` - Writes kernel to disk
  - `installer_install_bootloader()` - Installs bootloader to MBR
  - `installer_run()` - Orchestrates full installation

### Technical
- Created `kernel/installer/` directory for installer code
- Added `installer.h` header with installer API
- Implemented complete FAT12 filesystem creation
- Boot sector with BPB (BIOS Parameter Block)
- Dual FAT tables (FAT1 and FAT2)
- Root directory initialization
- User confirmation system with keyboard input
- Error handling for all installation steps

### Structure
- Organized installer in separate module
- Clean separation of concerns
- Modular installation steps
- Comprehensive error reporting

## [0.6.1] - 2026-03-08

### Added
- **Enhanced Text Editor**
  - Live status bar showing cursor position (line:column)
  - Character counter (current/max)
  - Save indicator (shows "SAVED" when file is saved)
  - Ctrl+S to save without exiting
  - Beautiful box-drawing UI with borders
  - Real-time position tracking
  - Buffer usage indicator
  - Professional layout with help text

### Changed
- Editor now shows line and column numbers in real-time
- Status bar updates on every keystroke
- Improved visual design with Unicode box characters
- Better user feedback for save operations

### Technical
- Added `terminal_get_row()`, `terminal_get_column()`, `terminal_set_cursor()` functions
- Implemented live cursor position tracking
- Status bar rendering system
- Ctrl+S keyboard shortcut support

## [0.6.0] - 2026-03-08

### Added
- **FAT12 Filesystem Support**
  - Read files from FAT12-formatted disks
  - Parse FAT12 boot sector and BPB (BIOS Parameter Block)
  - Read FAT table and root directory
  - Navigate cluster chains
  - File listing from disk
  - Automatic filesystem detection (FAT12 or ramfs fallback)
  - Foundation for persistent storage

### Changed
- Version bumped to 0.6.0 (major milestone: disk filesystem)
- Filesystem abstraction now detects FAT12 and uses it if available
- Falls back to ramfs if no FAT12 disk detected
- `ls` command shows files from disk if FAT12 available
- `cat` command reads files from disk if FAT12 available

### Technical
- Implemented FAT12 BPB parsing
- Implemented cluster chain traversal
- Implemented directory entry parsing
- FAT12 write operations planned for future release
- Hybrid mode: read from FAT12, write to ramfs (temporary)

### Limitations
- FAT12 write/delete not yet implemented (read-only for now)
- Maximum file size: 512 bytes for display
- Single cluster files only (multi-cluster support planned)

## [0.5.2] - 2026-03-08

### Added
- **Filesystem abstraction layer**
  - Generic filesystem interface: `fs_init()`, `fs_create()`, `fs_read()`, `fs_delete()`, `fs_list()`
  - Decouples applications from specific filesystem implementations
  - Shell and editor now use `fs_*` functions instead of `ramfs_*`
  - Easy to add new filesystems (FAT, ext2, etc.) without changing applications
  - Clean separation of concerns

### Changed
- Version bumped to 0.5.2
- Shell commands now use filesystem abstraction
- Editor now uses filesystem abstraction
- Kernel initialization uses `fs_init()` instead of `ramfs_init()`

### Technical
- Created `fs.h` and `fs.c` abstraction layer
- ramfs is now a backend implementation
- Future filesystems can be swapped by changing fs.c implementation

## [0.5.1] - 2026-03-08

### Added
- **Editor scrollback support**
  - PageUp/PageDown scrolling in text editor
  - View content that scrolled off screen while editing
  - Same 1-line scrolling as terminal
  - Updated editor header to show scroll instructions

### Changed
- Version bumped to 0.5.1
- Editor now supports terminal scrollback navigation

## [0.5.0] - 2026-03-08

### Added
- **Terminal scrollback buffer**
  - 1000 lines of history (80x1000 character buffer)
  - PageUp to scroll up (1 line at a time)
  - PageDown to scroll down (1 line at a time)
  - View previous output that scrolled off screen
  - Automatic scroll to bottom when typing
  - Color preservation in scrollback
  - Ring buffer when history limit reached

### Changed
- Version bumped to 0.5.0 (major feature: scrollback)
- VGA driver refactored to use buffer-render architecture
- Terminal no longer writes directly to VGA memory
- All output goes through terminal buffer first

### Technical
- Separated display buffer from VGA memory
- Implemented terminal_render() for buffer-to-VGA copying
- Added scroll_offset tracking for view position
- Automatic buffer shifting when history limit reached

## [0.4.2] - 2026-03-08

### Added
- **Simple text editor**
  - Edit files directly in the OS with `edit <filename>`
  - Maximum 512 bytes (same as ramfs file limit)
  - Basic operations: type, Enter for newline, Backspace to delete
  - ESC to save and exit
  - Loads existing files for editing
  - Perfect for testing filesystem and keyboard input
  - No arrow keys, no menus - just simple text editing

### Changed
- Version bumped to 0.4.2
- Updated help command with edit

## [0.4.1] - 2026-03-08

### Added
- **rm command**
  - Remove files from RAM filesystem
  - Usage: `rm <filename>`
  - Important for testing file slot reuse
- **touch command**
  - Create empty files
  - Usage: `touch <filename>`
  - Cleaner than using `write` for empty files

### Improved
- **RAM Filesystem (ramfs)**
  - Automatic null terminator added after file data (if space available)
  - Better text file handling for `cat` command
  - Size overflow properly validated (returns error for >512 bytes)
  - File overwrite works correctly when writing to existing files

### Changed
- Version bumped to 0.4.1
- Updated help command with rm and touch

## [0.4.0] - 2026-03-08

### Added
- **RAM Filesystem (ramfs)**
  - In-memory filesystem with 16 file slots
  - Maximum file size: 512 bytes
  - Maximum filename length: 32 characters
  - File operations: create, read, delete, list
- **ls command**
  - List all files in RAM filesystem
  - Shows filename and size in bytes
- **cat command**
  - Display file contents
  - Usage: `cat <filename>`
- **write command**
  - Create or overwrite files
  - Usage: `write <filename> <content>`
  - Content can include spaces

### Changed
- Version bumped to 0.4.0 (major feature: filesystem)
- Updated help command with filesystem commands

## [0.3.5] - 2026-03-08

### Added
- **diskinfo command**
  - Display ATA disk controller status
  - Shows controller port, availability, mode (PIO), and addressing (LBA28)
  - Helpful for checking if disk is detected
- **sconsole command**
  - Check serial console status with `sconsole --status`
  - Shows COM1 port configuration (baud rate, data bits, parity, stop bits)
  - Displays initialization status
  - Reminds about QEMU `-serial stdio` flag

### Changed
- Version bumped to 0.3.5
- Updated help command with new commands

## [0.3.4] - 2026-03-08

### Added
- **ATA PIO disk driver**
  - Primary ATA controller support (0x1F0-0x1F7 ports)
  - Sector read function: `ata_read_sector(lba, buffer)`
  - Sector write function: `ata_write_sector(lba, buffer)`
  - LBA28 addressing mode (supports up to 128GB disks)
  - Status checking and error handling
  - First step towards filesystem support

### Changed
- Version bumped to 0.3.4
- Kernel can now read and write disk sectors directly

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
