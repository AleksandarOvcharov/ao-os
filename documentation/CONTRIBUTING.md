# Contributing to AO OS

Thank you for your interest in contributing to AO OS!

## Development Setup

1. Fork the repository
2. Clone your fork
3. Install the required tools (see BUILD.md)
4. Create a feature branch: `git checkout -b feature-name`
5. Make your changes
6. Test your changes with `make run`
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
1. Add command handler in `kernel/shell.c`
2. Update the help command to include your new command
3. Test thoroughly

### Adding a New Driver
1. Create header file in `include/`
2. Create implementation in `kernel/drivers/`
3. Update Makefile to compile the new driver
4. Include and initialize in `kernel/kernel.c`

## Testing

Always test your changes with:
```bash
make clean
make
make run
```

Test in QEMU and at least one other virtualization platform if possible.

## Pull Request Guidelines

- Provide a clear description of the changes
- Reference any related issues
- Ensure the code compiles without warnings
- Test all affected functionality
- Keep commits atomic and well-described

## Questions?

Feel free to open an issue for discussion before starting major work.
