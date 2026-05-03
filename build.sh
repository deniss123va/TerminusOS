#!/bin/bash

set -e

# ==============================================
# Цвета для красивого вывода
# ==============================================

RESET='\033[0m'
BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
RED='\033[0;31m'

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

# Количество параллельных задач = число ядер процессора
NUM_JOBS=$(nproc 2>/dev/null || echo 4)
CURRENT_JOBS=0

# Таймер
BUILD_START=$(date +%s)

# Функция для форматирования времени
format_time() {
    local seconds=$1
    local hours=$((seconds / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    
    if [ $hours -gt 0 ]; then
        printf "%02d:%02d:%02d" $hours $minutes $secs
    else
        printf "%02d:%02d" $minutes $secs
    fi
}

# Функция для красивого заголовка
print_header() {
    echo -e "${BOLD}${CYAN}════════════════════════════════════════════${RESET}"
    echo -e "${BOLD}${CYAN}║ $1${RESET}"
    echo -e "${BOLD}${CYAN}════════════════════════════════════════════${RESET}"
}

# ==============================================
# Stage 1: Сборка ядра (параллельная компиляция)
# ==============================================

print_header "🔨 Stage 1: Building Kernel (${NUM_JOBS} cores)"

STAGE1_START=$(date +%s)

mkdir -p $BUILD_DIR/{kernel,lib,fs,shell,drivers/commands}

CPP_FILES=$(find src -name "*.cpp" -type f)
ASM_FILES=$(find src -name "*.s" -type f)

OBJ_LIST=""

# Функция для ожидания свободного слота
wait_for_slot() {
    while [ $CURRENT_JOBS -ge $NUM_JOBS ]; do
        wait -n 2>/dev/null || true
        CURRENT_JOBS=$((CURRENT_JOBS - 1))
    done
}

# Компилируем .s файлы (ассемблер) параллельно
ASM_COUNT=$(echo $ASM_FILES | wc -w)
CPP_COUNT=$(echo $CPP_FILES | wc -w)
TOTAL_FILES=$((ASM_COUNT + CPP_COUNT))

echo -e "${YELLOW}📦 Files to compile: ${BOLD}${TOTAL_FILES}${RESET} (${ASM_COUNT} asm + ${CPP_COUNT} cpp)"
echo -e "${BLUE}⚙️  Starting compilation...${RESET}"
echo ""

ASM_DONE=0
for SRC_FILE in $ASM_FILES; do
    OBJ_FILE=$(echo $SRC_FILE | sed "s|^src/|$BUILD_DIR/|;s|\.s$|\.o|")
    mkdir -p "$(dirname "$OBJ_FILE")"
    
    wait_for_slot
    $AS $SRC_FILE -o $OBJ_FILE &
    CURRENT_JOBS=$((CURRENT_JOBS + 1))
    OBJ_LIST="$OBJ_LIST $OBJ_FILE"
    ASM_DONE=$((ASM_DONE + 1))
done

CPP_DONE=0
for SRC_FILE in $CPP_FILES; do
    OBJ_FILE=$(echo $SRC_FILE | sed "s|^src/|$BUILD_DIR/|;s|\.cpp$|\.o|")
    mkdir -p "$(dirname "$OBJ_FILE")"
    
    wait_for_slot
    $CC $CFLAGS -c $SRC_FILE -o $OBJ_FILE &
    CURRENT_JOBS=$((CURRENT_JOBS + 1))
    OBJ_LIST="$OBJ_LIST $OBJ_FILE"
    CPP_DONE=$((CPP_DONE + 1))
done

# Ждём завершения всех компиляций
echo -e "${YELLOW}⏳ Waiting for compilation to finish...${RESET}"
wait

# Линковка ядра
echo -e "${YELLOW}🔗 Linking kernel...${RESET}"
$LD $LDFLAGS $OBJ_LIST -o $KERNEL_ELF

STAGE1_END=$(date +%s)
STAGE1_TIME=$((STAGE1_END - STAGE1_START))

echo -e "${GREEN}✓ Kernel built: $KERNEL_ELF${RESET}"
echo -e "${GREEN}✓ Stage 1 time: $(format_time $STAGE1_TIME)${RESET}"

# ==============================================
# Stage 2: Создание ISO (минимальный GRUB)
# ==============================================

echo ""
print_header "💿 Stage 2: Creating ISO Image"

STAGE2_START=$(date +%s)

if [ ! -f "$GRUB_CFG" ]; then
    echo -e "${RED}✗ ERROR: Missing $GRUB_CFG${RESET}"
    exit 1
fi

rm -rf $ISO_DIR
mkdir -p $ISO_DIR/boot/grub

echo -e "${BLUE}📁 Copying files...${RESET}"
cp $KERNEL_ELF $ISO_DIR/boot/kernel.elf
cp $GRUB_CFG   $ISO_DIR/boot/grub/grub.cfg

# Минимальный набор модулей
GRUB_MODULES="biosdisk iso9660 multiboot normal configfile"

echo -e "${BLUE}🎛️  Creating GRUB image...${RESET}"
grub-mkimage \
    -O i386-pc-eltorito \
    -o $ISO_DIR/boot/grub/core.img \
    -p /boot/grub \
    $GRUB_MODULES

echo -e "${BLUE}📀 Building ISO...${RESET}"
xorriso -as mkisofs \
    -R -J \
    -b boot/grub/core.img \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -o $ISO_FILE \
    $ISO_DIR 2>/dev/null

STAGE2_END=$(date +%s)
STAGE2_TIME=$((STAGE2_END - STAGE2_START))

echo -e "${GREEN}✓ ISO created: $ISO_FILE${RESET}"
echo -e "${GREEN}✓ Stage 2 time: $(format_time $STAGE2_TIME)${RESET}"

# ==============================================
# Финальная статистика
# ==============================================

BUILD_END=$(date +%s)
BUILD_TOTAL=$((BUILD_END - BUILD_START))

echo ""
print_header "📊 Build Statistics"

KERNEL_SIZE=$(du -h $KERNEL_ELF | cut -f1)
ISO_SIZE=$(du -h $ISO_FILE | cut -f1)

echo -e "${CYAN}📈 Files compiled:${RESET}        ${BOLD}$TOTAL_FILES${RESET}"
echo -e "${CYAN}📦 Kernel size:${RESET}           ${BOLD}$KERNEL_SIZE${RESET}"
echo -e "${CYAN}💿 ISO size:${RESET}              ${BOLD}$ISO_SIZE${RESET}"
echo ""
echo -e "${CYAN}⏱️  Stage 1 (compilation):${RESET}  ${BOLD}$(format_time $STAGE1_TIME)${RESET}"
echo -e "${CYAN}⏱️  Stage 2 (ISO):${RESET}         ${BOLD}$(format_time $STAGE2_TIME)${RESET}"
echo -e "${CYAN}⏱️  Total build time:${RESET}      ${BOLD}$(format_time $BUILD_TOTAL)${RESET}"

echo ""
echo -e "${GREEN}${BOLD}✓ Build completed successfully!${RESET}"
echo ""