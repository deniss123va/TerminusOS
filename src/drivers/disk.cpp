#include "disk.h"
#include "../kernel/screen.h" // Для println

// Константы ATA
#define ATA_PRIMARY_DATA 0x1F0
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW 0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HIGH 0x1F5
#define ATA_PRIMARY_DRIVE_HEAD 0x1F6
#define ATA_PRIMARY_COMMAND 0x1F7
#define ATA_PRIMARY_CONTROL 0x3F6

#define ATA_CMD_READ_PIO 0x20 
#define ATA_CMD_WRITE_PIO 0x30 

// **********************************************
// ===== 3. I/O Functions (Работа с портами ввода/вывода) =====
// **********************************************

uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %0, %1" : : "a"(data), "dN"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void outw(uint16_t port, uint16_t data) {
    asm volatile("outw %0, %1" : : "a"(data), "dN"(port));
}


// **********************************************
// ===== 5. ATA Driver (Чтение/Запись диска) =====
// **********************************************

uint8_t sector_buffer[512]; // Буфер для чтения/записи сектора

void ata_wait_for_ready() {
    while (inb(ATA_PRIMARY_COMMAND) & 0x80); 
    for (int i = 0; i < 4; i++) {
        inb(ATA_PRIMARY_COMMAND);
    }
    
    uint8_t status;
    do {
        status = inb(ATA_PRIMARY_COMMAND);
        if (status & 0x01 || status & 0x20) {
            println("ATA Error or Fault during wait!");
            return;
        }
    } while (!(status & 0x08)); 
}

void ata_read_sector(uint32_t lba) {
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_PRIMARY_SECTOR_COUNT, 1); 
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba); 
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8)); 
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16)); 
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    ata_wait_for_ready();

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)sector_buffer)[i] = inw(ATA_PRIMARY_DATA);
    }
}

void ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_PRIMARY_SECTOR_COUNT, 1); 
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba); 
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8)); 
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16)); 

    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
    ata_wait_for_ready(); 

    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_DATA, ((uint16_t*)buffer)[i]);
    }

    uint8_t status;
    do {
        status = inb(ATA_PRIMARY_COMMAND);
    } while (status & 0x80); 
    
    if (status & 0x01 || status & 0x20) {
        println("ATA Write Error!");
    }
}