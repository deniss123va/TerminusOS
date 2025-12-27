#!/bin/bash

set -e

# ==============================================
# Конфигурация
# ==============================================

CROSS=""
CC="${CROSS}g++"
AS="${CROSS}gcc -m32 -c -x assembler"
LD="${CROSS}ld"

BUILD_DIR="build"
ISO_DIR="isodir"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
GRUB_CFG="boot/grub/grub.cfg"
ISO_FILE="Terminus_OS.iso"

CFLAGS="-m32 -ffreestanding -O2 -std=gnu++17 -fno-exceptions -fno-rtti"
LDFLAGS="-m elf_i386 -T src/linker.ld -nostdlib"

# ==============================================
# Stage 1: Сборка ядра
# ==============================================

echo "=== Stage 1: Building kernel ==="

# Создаём все необходимые директории
mkdir -p $BUILD_DIR/{kernel,lib,fs,shell,drivers/commands}

# Автоматически находим все исходники
echo "Scanning for source files..."

CPP_FILES=$(find src -name "*.cpp" -type f)
ASM_FILES=$(find src -name "*.s" -type f)

OBJ_LIST=""

# Компилируем .s файлы (ассемблер)
for SRC_FILE in $ASM_FILES; do
    OBJ_FILE=$(echo $SRC_FILE | sed "s|^src/|$BUILD_DIR/|;s|\.s$|\.o|")
    
    echo "Assembling $SRC_FILE..."
    
    # Создаём директорию для объектника, если нужно
    mkdir -p "$(dirname "$OBJ_FILE")"
    
    $AS $SRC_FILE -o $OBJ_FILE
    OBJ_LIST="$OBJ_LIST $OBJ_FILE"
done

# Компилируем .cpp файлы
for SRC_FILE in $CPP_FILES; do
    OBJ_FILE=$(echo $SRC_FILE | sed "s|^src/|$BUILD_DIR/|;s|\.cpp$|\.o|")
    
    echo "Compiling $SRC_FILE..."
    
    # Создаём директорию для объектника, если нужно
    mkdir -p "$(dirname "$OBJ_FILE")"
    
    $CC $CFLAGS -c $SRC_FILE -o $OBJ_FILE
    OBJ_LIST="$OBJ_LIST $OBJ_FILE"
done

# Линковка ядра
echo "Linking $KERNEL_ELF..."
$LD $LDFLAGS $OBJ_LIST -o $KERNEL_ELF

echo "Kernel built: $KERNEL_ELF"

# ==============================================
# Stage 2: Создание ISO
# ==============================================

echo "=== Stage 2: Creating ISO ==="

if [ ! -f "$GRUB_CFG" ]; then
    echo "ERROR: Missing $GRUB_CFG"
    exit 1
fi

rm -rf $ISO_DIR
mkdir -p $ISO_DIR/boot/grub

cp $KERNEL_ELF $ISO_DIR/boot/kernel.elf
cp $GRUB_CFG $ISO_DIR/boot/grub/grub.cfg

grub-mkrescue -o $ISO_FILE $ISO_DIR 2>/dev/null

echo "ISO created: $ISO_FILE"

# ==============================================
# Статистика
# ==============================================

echo ""
echo "=== Build complete! ==="
echo "Total source files compiled: $(echo $CPP_FILES $ASM_FILES | wc -w)"
echo "Kernel size: $(du -h $KERNEL_ELF | cut -f1)"
echo "ISO size: $(du -h $ISO_FILE | cut -f1)"