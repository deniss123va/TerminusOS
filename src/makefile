CROSS=i386-elf-
CC=$(CROSS)g++
AS=$(CROSS)gcc -x assembler-with-c
LD=$(CROSS)ld
OBJCOPY=$(CROSS)objcopy

CFLAGS = -ffreestanding -O2 -std=gnu++17 -fno-exceptions -fno-rtti
LDFLAGS = -T linker.ld -nostdlib

# Все объектные файлы
OBJS = build/boot.o \
       build/kernel.o \
       build/screen.o \
       build/keyboard.o \
       build/interrupts.o \
       build/interrupts_asm.o \
       build/panic.o \
       build/disk.o \
       build/fat16.o \
       build/string.o \
       build/utils.o \
       build/shell.o \
       build/builtin.o \
       build/cmd_info.o \
       build/cmd_nano.o \
	   build/cmd_data.o \
       build/rtc.o

.PHONY: all clean iso run

all: build/kernel.elf

build:
	mkdir -p build

# Boot
build/boot.o: src/boot.s | build
	$(AS) -c src/boot.s -o build/boot.o

# Kernel
build/kernel.o: src/kernel/kernel.cpp | build
	$(CC) $(CFLAGS) -c src/kernel/kernel.cpp -o build/kernel.o

# Screen
build/screen.o: src/kernel/screen.cpp | build
	$(CC) $(CFLAGS) -c src/kernel/screen.cpp -o build/screen.o

# Keyboard
build/keyboard.o: src/kernel/keyboard.cpp | build
	$(CC) $(CFLAGS) -c src/kernel/keyboard.cpp -o build/keyboard.o

# Interrupts
build/interrupts.o: src/kernel/interrupts.cpp | build
	$(CC) $(CFLAGS) -c src/kernel/interrupts.cpp -o build/interrupts.o

build/interrupts_asm.o: src/kernel/interrupts_asm.s | build
	$(AS) -c src/kernel/interrupts_asm.s -o build/interrupts_asm.o

# Panic
build/panic.o: src/kernel/panic.cpp | build
	$(CC) $(CFLAGS) -c src/kernel/panic.cpp -o build/panic.o

# Disk
build/disk.o: src/drivers/disk.cpp | build
	$(CC) $(CFLAGS) -c src/drivers/disk.cpp -o build/disk.o

# RTC (новый драйвер)
build/rtc.o: src/drivers/rtc.cpp | build
	$(CC) $(CFLAGS) -c src/drivers/rtc.cpp -o build/rtc.o

# FAT16
build/fat16.o: src/fs/fat16.cpp | build
	$(CC) $(CFLAGS) -c src/fs/fat16.cpp -o build/fat16.o

# String
build/string.o: src/lib/string.cpp | build
	$(CC) $(CFLAGS) -c src/lib/string.cpp -o build/string.o

# Utils
build/utils.o: src/lib/utils.cpp | build
	$(CC) $(CFLAGS) -c src/lib/utils.cpp -o build/utils.o

# Shell
build/shell.o: src/shell/shell.cpp | build
	$(CC) $(CFLAGS) -c src/shell/shell.cpp -o build/shell.o

# Builtin
build/builtin.o: src/shell/builtin.cpp | build
	$(CC) $(CFLAGS) -c src/shell/builtin.cpp -o build/builtin.o

# Commands
build/cmd_info.o: src/drivers/commands/cmd_info.cpp | build
	$(CC) $(CFLAGS) -c src/drivers/commands/cmd_info.cpp -o build/cmd_info.o

build/cmd_nano.o: src/drivers/commands/cmd_nano.cpp | build
	$(CC) $(CFLAGS) -c src/drivers/commands/cmd_nano.cpp -o build/cmd_nano.o

# Linking
build/kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o build/kernel.elf

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
