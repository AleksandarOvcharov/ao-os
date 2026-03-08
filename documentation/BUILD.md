# Building AO OS

## Prerequisites

### Windows (WSL2 or MSYS2)

You'll need to install the following tools:

#### Using WSL2 (Recommended)
```bash
sudo apt update
sudo apt install build-essential nasm grub-pc-bin xorriso qemu-system-x86
```

#### Installing GCC Cross-Compiler for i686-elf

```bash
# Install dependencies
sudo apt install libgmp3-dev libmpfr-dev libisl-dev libmpc-dev texinfo

# Download and build binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.39.tar.gz
tar -xzf binutils-2.39.tar.gz
cd binutils-2.39
mkdir build && cd build
../configure --target=i686-elf --prefix=/usr/local --disable-nls --disable-werror
make -j$(nproc)
sudo make install
cd ../..

# Download and build GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.gz
tar -xzf gcc-12.2.0.tar.gz
cd gcc-12.2.0
mkdir build && cd build
../configure --target=i686-elf --prefix=/usr/local --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
sudo make install-gcc
sudo make install-target-libgcc
cd ../..
```

### Linux

```bash
sudo apt update
sudo apt install build-essential nasm grub-pc-bin xorriso qemu-system-x86
```

Then follow the cross-compiler installation steps above.

### macOS

```bash
brew install nasm i686-elf-gcc i686-elf-binutils xorriso qemu
brew install grub --with-i386-elf
```

## Building the OS

Once all prerequisites are installed:

```bash
make
```

This will:
1. Compile the bootloader (boot.asm)
2. Compile all kernel C files
3. Link everything together
4. Create a bootable ISO image (ao-os.iso)

## Running the OS

### With QEMU (Recommended for testing)
```bash
make run
```

### With VirtualBox
1. Create a new VM with type "Other/Unknown"
2. Allocate at least 32MB of RAM
3. Mount `ao-os.iso` as a CD-ROM
4. Boot the VM

### With VMware
1. Create a new VM
2. Select "I will install the operating system later"
3. Choose "Other" as the guest OS
4. Mount `ao-os.iso` as a CD-ROM
5. Boot the VM

## Cleaning Build Artifacts

```bash
make clean
```

## Troubleshooting

### "grub-mkrescue: command not found"
Install GRUB tools:
```bash
sudo apt install grub-pc-bin grub-common
```

### "i686-elf-gcc: command not found"
You need to install the cross-compiler. Follow the installation steps above.

### ISO boots but shows GRUB rescue prompt
Make sure GRUB configuration is correct in the Makefile and that the kernel binary is properly placed in the ISO directory.
