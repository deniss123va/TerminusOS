#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stdbool.h>

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

// НОВАЯ СТРУКТУРА ДЛЯ РЕЗУЛЬТАТА ПОИСКА (исправляет ошибку компиляции)
struct FAT_FindResult {
    bool found;
    uint32_t lba; // LBA сектора, где найдена запись
    int index;    // Индекс записи в секторе (0-15)
    FAT16_Entry entry; // КОПИЯ записи (нужно для функций, которые не меняют диск)
};

extern "C" {
    uint32_t fat_cluster_to_lba(uint16_t cluster);
    uint16_t fat_get_next_cluster(uint16_t current_cluster);
    uint16_t fat_find_free_cluster_and_mark(uint16_t mark_val);
    void fat_format_name(const char* input, char* output_8_3);
    
    // Directory Operations
    // ОБНОВЛЕННАЯ СИГНАТУРА: возвращает FAT_FindResult (исправляет ошибку в fat16.cpp:94)
    FAT_FindResult fat_find_entry(const char* name, uint8_t target_attr); 
    
    // СТАРЫЙ АНАЛОГ: возвращает указатель на запись (для модификации)
    FAT16_Entry* fat_find_entry_for_modification(const char* name, uint32_t* lba_out, int* index_out, uint8_t target_attr);
    
    FAT16_Entry* fat_find_free_dir_entry(uint32_t* lba_out);
    void fat_delete_cluster_chain(uint16_t start_cluster);
    
    // Modification Logic
    bool fat_create_file(const char* name, const char* content, uint32_t size);
    void fat_create_dir(char* name);
    void fat_delete_entry_data(char* name, uint8_t target_attr);
    void fat_rename_entry(char* old_name, char* new_name);
}

#endif // FAT16_H