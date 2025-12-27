#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // Для NULL

// Глобальное состояние FAT16
extern uint16_t current_dir_cluster;

// Константы FAT16
#define FAT_ROOT_DIR_CLUSTER 0
#define FAT_SECTORS_PER_CLUSTER 1
#define FAT_RESERVED_SECTORS 2
#define FAT_NUMBER_OF_FATS 2
#define FAT_SECTORS_PER_FAT 20
#define FAT_ROOT_DIR_ENTRIES 512
#define FAT_BYTES_PER_SECTOR 512
#define FAT_ENTRY_SIZE 32

// Вычисленные адреса LBA
#define FAT_START_SECTOR (FAT_RESERVED_SECTORS)
#define ROOT_DIR_START_SECTOR (FAT_START_SECTOR + (FAT_NUMBER_OF_FATS * FAT_SECTORS_PER_FAT))
#define FAT_ROOT_DIR_SECTORS ((FAT_ROOT_DIR_ENTRIES * FAT_ENTRY_SIZE) / FAT_BYTES_PER_SECTOR)
#define DATA_START_SECTOR (ROOT_DIR_START_SECTOR + FAT_ROOT_DIR_SECTORS)

struct FAT_BPB {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed));

extern FAT_BPB fat_bpb;

// Структура записи FAT16
struct FAT16_Entry {
    char name[8];
    char ext[3];
    uint8_t attributes;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_cluster;
    uint32_t file_size;
} __attribute__((packed));

struct FAT_FindResult {
    bool found;
    struct FAT16_Entry entry;
    uint32_t lba;
    int index;
    FAT16_Entry* entry_ptr;
};

extern "C" {

// ===== ИНИЦИАЛИЗАЦИЯ =====
void fat_init();  // ПЕРЕМЕСТИЛ ВНУТРЬ extern "C"

// ===== БАЗОВЫЕ ФУНКЦИИ =====
void fat_write_cluster_data(
    uint16_t cluster,
    const uint8_t* data,
    uint32_t size,
    uint32_t offset
);

uint32_t fat_cluster_to_lba(uint16_t cluster);
uint32_t fat_get_total_sectors();
uint16_t fat_get_next_cluster(uint16_t current_cluster);

// ИСПРАВЛЕНО: удалено старое fat_find_free_cluster_and_mark
uint16_t fat_find_free_cluster();  // НОВОЕ: без автоматической пометки
void fat_set_entry(uint16_t cluster, uint16_t value);  // НОВОЕ: для ручной установки

void fat_format_name(const char* input, char* output_8_3);

// ===== ПОИСК =====
FAT_FindResult fat_find_entry(const char* name, uint8_t target_attr);
FAT16_Entry* fat_find_entry_for_modification(const char* name, uint32_t* lba_out, int* index_out, uint8_t target_attr);
FAT16_Entry* fat_find_free_dir_entry(uint32_t* lba_out);

// ===== УДАЛЕНИЕ =====
void fat_debug_check_chain(uint16_t start_cluster);
void fat_delete_cluster_chain(uint16_t start_cluster);
void fat_delete_entry_data(char* name, uint8_t target_attr);
void fat_debug_cluster_state(uint16_t cluster);

// ===== СОЗДАНИЕ И ПЕРЕИМЕНОВАНИЕ =====
bool fat_create_file(const char* name, const char* content, uint32_t size);
void fat_create_dir(char* name);
void fat_rename_entry(char* old_name, char* new_name);

}

#endif // FAT16_H