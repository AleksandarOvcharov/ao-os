# Changelog

All notable changes to AO OS will be documented in this file.

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
