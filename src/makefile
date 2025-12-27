CROSS=i386-elf-
CC=$(CROSS)g++
AS=$(CROSS)gcc -x assembler-with-c
LD=$(CROSS)ld
OBJCOPY=$(CROSS)objcopy

CFLAGS = -ffreestanding -O2 -std=gnu++17 -fno-exceptions -fno-rtti
LDFLAGS = -T linker.ld -nostdlib

SRC = src/boot.S src/kernel.cpp
OBJ = build/boot.o build/kernel.o

.PHONY: all clean iso run

all: build/kernel.elf

build:
	mkdir -p build

build/boot.o: src/boot.S | build
	$(AS) -c src/boot.S -o build/boot.o

build/kernel.o: src/kernel.cpp | build
	$(CC) $(CFLAGS) -c src/kernel.cpp -o build/kernel.o

build/kernel.elf: build/boot.o build/kernel.o linker.ld
	$(LD) $(LDFLAGS) build/boot.o build/kernel.o -o build/kernel.elf

iso: all
	# подготовим структуру isodir
	rm -rf isodir
	mkdir -p isodir/boot/grub
	cp build/kernel.elf isodir/boot/kernel.elf
	cp boot/grub/grub.cfg isodir/boot/grub/grub.cfg
	# создаём ISO (grub-mkrescue требует xorriso)
	grub-mkrescue -o myos.iso isodir

run: iso
	qemu-system-i386 -cdrom myos.iso

clean:
	rm -rf build isodir myos.iso
