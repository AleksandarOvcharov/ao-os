# Building AO OS

## Prerequisites

AO OS uses a custom bootloader (no GRUB) and targets x86-64 (64-bit).

### Required Tools
- `x86_64-elf-gcc` — GCC cross-compiler for x86-64
- `x86_64-elf-binutils` — Binutils (linker, objcopy)
- `nasm` — Netwide Assembler
- `make` — GNU Make
- `qemu-system-x86_64` — QEMU for testing

### Linux (apt-based)
```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86
```

Then install the x86_64-elf cross-compiler:
```bash
# Install dependencies
sudo apt install libgmp3-dev libmpfr-dev libisl-dev libmpc-dev texinfo

# Download and build binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.gz
tar -xzf binutils-2.42.tar.gz
cd binutils-2.42
mkdir build && cd build
../configure --target=x86_64-elf --prefix=/usr/local --disable-nls --disable-werror
make -j$(nproc)
sudo make install
cd ../..

# Download and build GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-14.1.0/gcc-14.1.0.tar.gz
tar -xzf gcc-14.1.0.tar.gz
cd gcc-14.1.0
mkdir build && cd build
../configure --target=x86_64-elf --prefix=/usr/local --disable-nls --enable-languages=c --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
sudo make install-gcc
sudo make install-target-libgcc
cd ../..
```

### macOS (Homebrew)
```bash
brew install nasm x86_64-elf-gcc x86_64-elf-binutils qemu
```

### Windows (MSYS2)
Install MSYS2, then in the MSYS2 terminal:
```bash
pacman -S nasm make qemu
```
Install x86_64-elf-gcc via Homebrew on WSL2, or build from source.

## Building the OS

```bash
make
```

This will:
1. Assemble the 2-stage bootloader (stage1.asm, stage2.asm)
2. Assemble kernel entry and interrupt handlers (boot.asm, interrupt.asm, gdt.asm)
3. Compile all kernel C files
4. Link the kernel ELF and extract flat binary
5. Create a raw bootable disk image (ao-os.iso)

### Disk Image Layout
| Sector(s) | Contents |
|-----------|----------|
| 0 | Stage 1 bootloader (MBR, 512 bytes) |
| 1-4 | Stage 2 bootloader (2 KB) |
| 5+ | Kernel flat binary |

## Running the OS

### With QEMU (Recommended)
```bash
make run          # Boot with serial console output
make run-disk     # Boot with a second FAT12 floppy disk
```

### With VirtualBox
1. Create a new VM with type "Other/Unknown (64-bit)"
2. Allocate at least 32 MB of RAM
3. Add `ao-os.iso` as a hard disk image (raw format)
4. Boot the VM

### With VMware
1. Create a new VM, select "Other 64-bit"
2. Add `ao-os.iso` as a disk image
3. Boot the VM

## Cleaning Build Artifacts

```bash
make clean
```

## Troubleshooting

### "x86_64-elf-gcc: command not found"
You need to install the cross-compiler. Follow the installation steps above.

### OS boots but no keyboard input
Make sure QEMU is not capturing keyboard in a weird mode. Try clicking inside the QEMU window. The keyboard driver uses IRQ1 interrupts.

### Triple fault on boot
Check the serial console output (`-serial stdio`) for exception details. The kernel now handles all CPU exceptions and displays register dumps.
