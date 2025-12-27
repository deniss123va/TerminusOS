#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdbool.h>

// I/O Functions
extern "C" uint8_t inb(uint16_t port);
extern "C" void outb(uint16_t port, uint8_t data);
extern "C" uint16_t inw(uint16_t port);
extern "C" void outw(uint16_t port, uint16_t data);

// ATA Driver
extern uint8_t sector_buffer[512]; // Буфер для чтения/записи сектора

extern "C" {
    void ata_wait_for_ready();
    void ata_read_sector(uint32_t lba);
    void ata_write_sector(uint32_t lba, const uint8_t* buffer);
}

#endif // DISK_H