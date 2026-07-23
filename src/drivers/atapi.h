#ifndef ATAPI_H
#define ATAPI_H

#include <stdint.h>
#include <stdbool.h>

// ===== ATAPI (CD-ROM) Driver =====
// Использует вторичный канал ATA (0x170 / 0x376) для ATAPI-устройств
// Поддерживает чтение секторов 2048 байт (CD-ROM Mode 1/2)

#define ATAPI_SECTOR_SIZE     2048

// Статус детекта диска
extern bool atapi_disk_present;

extern "C" {

// Инициализация: определяет наличие ATAPI-устройства
// Возвращает true если устройство найдено
bool atapi_detect();

// Проверяет наличие диска в приводе
// Возвращает true если диск вставлен
bool atapi_check_disk();

// Читает один сектор CD-ROM (2048 байт) в buf
// lba — логический номер сектора (0 = первый сектор диска)
// Возвращает true при успехе
bool atapi_read_sector(uint32_t lba, uint8_t* buf);

} // extern "C"

#endif // ATAPI_H
