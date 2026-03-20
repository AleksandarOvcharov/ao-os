# Makefile for AO-OS C Examples
# Compiles C programs to raw binaries and AOB executables

CC = x86_64-elf-gcc
CFLAGS = -std=c99 -ffreestanding -O2 -nostdlib -fno-builtin -fno-stack-protector -fno-pic \
         -mno-red-zone -mcmodel=kernel -mno-sse -mno-mmx -mno-sse2 -I../include
LDFLAGS = -nostdlib -Wl,--oformat=binary -Wl,-Ttext=0x200000

AOBCOMPILE = ../tools/aobcompile

# Source files
SOURCES = $(wildcard *.c)
BINARIES = $(SOURCES:.c=.bin)
AOBS = $(SOURCES:.c=.aob)

.PHONY: all clean bin aob

all: bin aob

bin: $(BINARIES)

aob: $(AOBS)

%.bin: %.c ../include/ao.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

%.aob: %.bin
	$(AOBCOMPILE) $< $@ "$(basename $<)"

clean:
	rm -f *.bin *.aob *.o

help:
	@echo "AO-OS C Examples Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build both .bin and .aob files (default)"
	@echo "  bin     - Build only .bin files"
	@echo "  aob     - Build only .aob files (requires .bin first)"
	@echo "  clean   - Remove all built files"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build everything"
	@echo "  make bin      # Build raw binaries"
	@echo "  make aob      # Build AOB executables"
	@echo "  make clean    # Clean build files"
