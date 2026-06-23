# Contributing to AO OS

Thank you for your interest in contributing to AO OS!

## Development Setup

1. Fork the repository
2. Clone your fork
3. Install the required tools (see BUILD.md)
4. Create a feature branch: `git checkout -b feature-name`
5. Make your changes
6. Build and test your changes (see the matrix below)
7. Commit your changes: `git commit -am 'Add feature'`
8. Push to your fork: `git push origin feature-name`
9. Create a Pull Request

## Code Style

### C Code

- Use 4 spaces for indentation (no tabs)
- Opening braces on the same line for functions
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused and small

### Assembly Code

- Use meaningful labels
- Comment non-obvious instructions
- Follow the existing style in boot.asm

## Adding New Features

### Adding a New Command

1. Add the command handler in `kernel/shell/commands.c`
2. Add command dispatch in `kernel/shell.c`
3. Update the help command to include your new command
4. Test thoroughly

### Adding a New Driver

1. Create header file in `include/`
2. Create implementation in `kernel/drivers/`
3. Update Makefile to compile the new driver
4. Include and initialize in `kernel/kernel.c`

### Changing Storage or Filesystems

1. Keep the generic interface in `include/fs.h` independent of FAT internals
2. Update the shared FAT implementation in `kernel/fs/fat.c`
3. Preserve both FAT copies and FAT32 FSInfo metadata on allocation/free paths
4. Add a regression case to `tools/tests/fat_host_test.c`
5. Run `make test-fat` before boot testing

## Testing

Always test your changes with:

```bash
make clean
make
make test-fat
make run
```

`make test-fat` requires `dosfstools` and `mtools`. It exercises FAT12, FAT16,
and FAT32 images, multi-cluster I/O, cluster reuse, expanding directories, and
corrupted FAT chains. A storage-related pull request should include this test;
other changes should run it whenever they touch ATA, filesystem, installer, or
boot-sector code.

Test in QEMU and at least one other virtualization platform if possible.

## Pull Request Guidelines

- Provide a clear description of the changes
- Reference any related issues
- Ensure the code compiles without warnings
- Test all affected functionality
- Keep README, BUILD, FEATURES, and CHANGELOG synchronized with user-visible changes
- Keep commits atomic and well-described

## Questions?

Feel free to open an issue for discussion before starting major work.
