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
extern uint8_t sector_buffer[512];

extern "C" {
    void ata_init();
    // Возвращает true при успехе. Внутри уже ретраит несколько раз на
    // транзиентных ошибках — существующие вызовы, игнорирующие результат,
    // продолжают работать как раньше (сигнатура совместима "по значению").
    bool ata_read_sector(uint32_t lba);
    void ata_write_sector(uint32_t lba, const uint8_t* buffer);
    // Спрашивает у контроллера реальный размер диска (IDENTIFY DEVICE).
    // Возвращает true и пишет число адресуемых LBA28-секторов в *out_total_sectors,
    // либо false (нет диска / ATAPI вместо ATA / ошибка) — тогда *out_total_sectors не трогается.
    bool ata_identify(uint32_t* out_total_sectors);
}

#endif // DISK_H