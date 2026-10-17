# ====================================================================
# VanyOS Professional GUI Build Automation Tool
# ====================================================================

# Toolchain definitions
CC = i686-linux-gnu-gcc
AS = nasm
LD = i686-linux-gnu-ld

# Compiler configuration flags for strict bare-metal freestanding binaries
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -O2

# Linker configuration forcing layout compliance through our linker script
LDFLAGS = -m elf_i386 -T linker.ld

.PHONY: all run clean

# Default target creates the bootable OS installation image
all: os.iso

# Compile 32-bit Assembly entry point initialization layer
boot.o: boot.asm
	$(AS) -f elf32 boot.asm -o boot.o

# Compile the core C graphics kernel and mouse event loops
kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

# Link object blocks into a single elf execution binary using the linker script map
myos.bin: boot.o kernel.o linker.ld
	$(LD) $(LDFLAGS) boot.o kernel.o -o myos.bin

# Pack the finalized binary into a standard GRUB multi-system bootable ISO image
os.iso: myos.bin grub.cfg
	mkdir -p iso/boot/grub
	cp myos.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o os.iso iso

# Launch emulation wrapper via QEMU with mouse inputs captured natively
run: os.iso
	qemu-system-i386 -cdrom os.iso -m 256M

# Wipe transient object structures and artifact generation paths clean
clean:
	rm -rf *.o myos.bin os.iso iso
