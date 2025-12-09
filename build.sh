#!/bin/bash
set -e

# ==============================================
# КОНФИГУРАЦИЯ И ИНСТРУМЕНТАРИЙ
# ==============================================
# Кросс-компилятор (оставьте пустым, если используете локальный GCC с флагом -m32)
CROSS=""
CC="${CROSS}g++"
# Используем GCC для компиляции ассемблера с флагом -m32 для 32-битного вывода
AS="${CROSS}gcc -m32 -c -x assembler" 
LD="${CROSS}ld"

# ==============================================
# НАСТРОЙКИ ПРОЕКТА
# ==============================================
BUILD_DIR="build"
ISO_DIR="isodir"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
GRUB_CFG="boot/grub/grub.cfg"
ISO_FILE="Terminus_OS.iso" # Финальное имя образа

# Флаги компиляции C++
CFLAGS="-m32 -ffreestanding -O2 -std=gnu++17 -fno-exceptions -fno-rtti"
# Флаги линковщика (linker.ld находится в корне src/)
LDFLAGS="-m elf_i386 -T src/linker.ld -nostdlib" 

# Список всех исходных файлов для компиляции, на основе структуры src/
# Этот список устранит все ошибки 'undefined reference'.
ALL_SOURCES=(
    # Основные файлы (имена файлов соответствуют структуре )
    src/boot.s
    src/interrupts_asm.s
    
    # Kernel & Core
    src/kernel/kernel.cpp
    src/kernel/interrupts.cpp
    src/kernel/keyboard.cpp
    src/kernel/panic.cpp
    src/kernel/screen.cpp
    
    # Lib (Библиотечные функции: strlen, strcmp, strcpy и т.д.)
    src/lib/string.cpp
    src/lib/utils.cpp

    # File System
    src/fs/fat16.cpp
    src/fs/vfs.cpp
    
    # Shell & Commands
    src/shell/builtin.cpp
    src/shell/shell.cpp
    src/drivers/commands/cmd_info.cpp
    src/drivers/commands/cmd_nano.cpp
    
    # Drivers
    src/drivers/disk.cpp
)

# ==============================================
# ЭТАП 1: СБОРКА ЯДРА (ELF)
# ==============================================
echo "=== Stage 1: Building kernel ==="
# Создаем все необходимые директории
mkdir -p $BUILD_DIR $BUILD_DIR/kernel $BUILD_DIR/lib $BUILD_DIR/fs $BUILD_DIR/shell $BUILD_DIR/drivers $BUILD_DIR/drivers/commands

OBJ_LIST=""

for SRC_FILE in "${ALL_SOURCES[@]}"; do
    # Заменяем src/ на build/ и расширение на .o
    OBJ_FILE=$(echo $SRC_FILE | sed "s|^src/|$BUILD_DIR/|;s|\.s$|\.o|;s|\.cpp$|\.o|")
    
    echo "Compiling $SRC_FILE..."

    if [ -f "$SRC_FILE" ]; then
        if [[ "$SRC_FILE" == *.s ]]; then
            # Ассемблерные файлы (используем $AS с -m32)
            $AS $SRC_FILE -o $OBJ_FILE
        else
            # C/C++ файлы (используем $CC с $CFLAGS)
            $CC $CFLAGS -c $SRC_FILE -o $OBJ_FILE
        fi
        
        OBJ_LIST="$OBJ_LIST $OBJ_FILE"
    else
        echo "ERROR: Missing file $SRC_FILE. Check your file structure."
        exit 1
    fi
done

# 1.4 Линковка в kernel.elf
echo "Linking $KERNEL_ELF..."
# Используем полный список объектных файлов
$LD $LDFLAGS $OBJ_LIST -o $KERNEL_ELF

echo "Kernel built: $KERNEL_ELF"

# ==============================================
# ЭТАП 2: СБОРКА ISO С GRUB
# ==============================================
echo "=== Stage 2: Creating ISO ==="

if [ ! -f "$GRUB_CFG" ]; then
    echo "ERROR: Missing $GRUB_CFG. Please create the file: boot/grub/grub.cfg"
    exit 1
fi

rm -rf $ISO_DIR
mkdir -p $ISO_DIR/boot/grub

cp $KERNEL_ELF $ISO_DIR/boot/kernel.elf
cp $GRUB_CFG $ISO_DIR/boot/grub/grub.cfg

# Создаём ISO с финальным именем
grub-mkrescue -o $ISO_FILE $ISO_DIR

echo "ISO created: $ISO_FILE"

# ==============================================
# ЭТАП 3: Удален
# ==============================================