#include "fat16.h"
#include "../drivers/disk.h"
#include "../kernel/screen.h"
#include "../lib/string.h"
#include <stdint.h>

// Глобальное состояние
uint16_t current_dir_cluster = 0;
FAT_BPB fat_bpb;

extern "C" {

// **********************************************
// ===== Core Helpers =====
// **********************************************

void fat_format_name(const char* input, char* output_8_3) {
    for (int i = 0; i < 11; i++) output_8_3[i] = ' ';
    
    int src_i = 0;
    int dest_i = 0;
    
    // Имя
    while (input[src_i] != 0 && input[src_i] != '.' && dest_i < 8) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output_8_3[dest_i++] = c;
    }
    
    // Расширение
    while (input[src_i] != 0 && input[src_i] != '.') src_i++;
    if (input[src_i] == '.') src_i++;
    dest_i = 8;
    while (input[src_i] != 0 && dest_i < 11) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output_8_3[dest_i++] = c;
    }
}

uint32_t fat_cluster_to_lba(uint16_t cluster) {
    if (cluster == FAT_ROOT_DIR_CLUSTER) return ROOT_DIR_START_SECTOR;
    return DATA_START_SECTOR + (cluster - 2) * FAT_SECTORS_PER_CLUSTER;
}

uint16_t fat_get_next_cluster(uint16_t current_cluster) {
    if (current_cluster < 2 || current_cluster >= 0xFFF0) return 0xFFFF;
    
    uint32_t fat_sector_lba = FAT_START_SECTOR + (current_cluster / 256);
    uint16_t fat_entry_offset = current_cluster % 256;
    
    ata_read_sector(fat_sector_lba);
    uint16_t next_cluster = ((uint16_t*)sector_buffer)[fat_entry_offset];
    
    return next_cluster;
}

// Инициализация FAT
void fat_init() {
    println("FAT INIT: Initializing FAT table");
    
    ata_read_sector(FAT_START_SECTOR);
    uint16_t* fat_table = (uint16_t*)sector_buffer;
    
    if (fat_table[0] == 0x0000 && fat_table[1] == 0x0000) {
        println("FAT INIT: FAT is empty, initializing...");
        
        fat_table[0] = 0xFFF8;  // Media descriptor
        fat_table[1] = 0xFFFF;  // EOC marker
        
        ata_write_sector(FAT_START_SECTOR, sector_buffer);
        
        if (FAT_NUMBER_OF_FATS > 1) {
            ata_write_sector(FAT_START_SECTOR + FAT_SECTORS_PER_FAT, sector_buffer);
        }
        
        println("FAT INIT: Done");
    } else {
        println("FAT INIT: FAT already initialized");
    }
}

// ИСПРАВЛЕНО: Установка значения в FAT с локальным буфером
void fat_set_entry(uint16_t cluster, uint16_t value) {
    if (cluster < 2) {
        println("ERROR: Cannot modify reserved clusters");
        return;
    }
    
    uint32_t fat_sector_offset = cluster / 256;
    uint16_t fat_entry_index = cluster % 256;
    uint32_t fat_sector_lba = FAT_START_SECTOR + fat_sector_offset;
    
    // Локальный буфер для избежания конфликтов
    static uint8_t fat_buffer[512];
    
    ata_read_sector(fat_sector_lba);
    
    // Копируем в локальный буфер
    for (int i = 0; i < 512; i++) {
        fat_buffer[i] = sector_buffer[i];
    }
    
    uint16_t* fat_table = (uint16_t*)fat_buffer;
    
    // Отладка
    print("FAT SET: Cluster 0x");
    print_hex_byte((cluster >> 8) & 0xFF);
    print_hex_byte(cluster & 0xFF);
    print(" from 0x");
    print_hex_byte((fat_table[fat_entry_index] >> 8) & 0xFF);
    print_hex_byte(fat_table[fat_entry_index] & 0xFF);
    print(" to 0x");
    print_hex_byte((value >> 8) & 0xFF);
    print_hex_byte(value & 0xFF);
    print_char('\n');
    
    fat_table[fat_entry_index] = value;
    
    // Копируем обратно для записи
    for (int i = 0; i < 512; i++) {
        sector_buffer[i] = fat_buffer[i];
    }
    
    // Записываем оба экземпляра FAT
    ata_write_sector(fat_sector_lba, sector_buffer);
    
    if (FAT_NUMBER_OF_FATS > 1) {
        ata_write_sector(fat_sector_lba + FAT_SECTORS_PER_FAT, sector_buffer);
    }
    
    // Проверка записи
    ata_read_sector(fat_sector_lba);
    fat_table = (uint16_t*)sector_buffer;
    
    if (fat_table[fat_entry_index] != value) {
        println("ERROR: FAT write verification failed!");
    }
}

// Поиск свободного кластера
uint16_t fat_find_free_cluster() {
    for (uint16_t cluster = 2; cluster < 4096; cluster++) {
        uint32_t fat_sector_offset = cluster / 256;
        uint16_t fat_entry_index = cluster % 256;
        uint32_t fat_sector_lba = FAT_START_SECTOR + fat_sector_offset;
        
        ata_read_sector(fat_sector_lba);
        uint16_t* fat_table = (uint16_t*)sector_buffer;
        
        if (fat_table[fat_entry_index] == 0x0000) {
            return cluster;
        }
    }
    
    return 0;
}

// **********************************************
// ===== Directory Operations =====
// **********************************************

FAT16_Entry* fat_find_entry_for_modification(const char* name, uint32_t* lba_out, int* index_out, uint8_t target_attr) {
    char name_8_3[11];
    if (strcmp(name, ".") == 0) strcpy(name_8_3, ".          ");
    else if (strcmp(name, "..") == 0) strcpy(name_8_3, "..         ");
    else fat_format_name(name, name_8_3);
    
    uint16_t cluster = current_dir_cluster;
    
    // Корневая директория
    if (cluster == FAT_ROOT_DIR_CLUSTER) {
        for (uint32_t sec = 0; sec < FAT_ROOT_DIR_SECTORS; sec++) {
            uint32_t lba = ROOT_DIR_START_SECTOR + sec;
            ata_read_sector(lba);
            
            for (int i = 0; i < 16; i++) {
                FAT16_Entry* e = (FAT16_Entry*)(sector_buffer + i * FAT_ENTRY_SIZE);
                if (e->name[0] == 0x00) return NULL;
                if (e->name[0] == 0xE5 || e->attributes == 0x0F) continue;
                if (target_attr != 0 && !(e->attributes & target_attr)) continue;
                
                if (strncmp(e->name, name_8_3, 11) == 0) {
                    *lba_out = lba;
                    *index_out = i;
                    return e;
                }
            }
        }
        return NULL;
    }
    
    // Подкаталоги
    uint16_t c = cluster;
    while (c < 0xFFF8) {
        uint32_t lba = fat_cluster_to_lba(c);
        ata_read_sector(lba);
        
        for (int i = 0; i < 16; i++) {
            FAT16_Entry* e = (FAT16_Entry*)(sector_buffer + i * FAT_ENTRY_SIZE);
            if (e->name[0] == 0x00) return NULL;
            if (e->name[0] == 0xE5 || e->attributes == 0x0F) continue;
            if (target_attr != 0 && !(e->attributes & target_attr)) continue;
            
            if (strncmp(e->name, name_8_3, 11) == 0) {
                *lba_out = lba;
                *index_out = i;
                return e;
            }
        }
        
        c = fat_get_next_cluster(c);
    }
    
    return NULL;
}

FAT_FindResult fat_find_entry(const char* name, uint8_t target_attr) {
    FAT_FindResult result = {0};
    uint32_t lba = 0;
    int index = 0;
    
    FAT16_Entry* entry_ptr = fat_find_entry_for_modification(name, &lba, &index, target_attr);
    
    if (entry_ptr != NULL) {
        result.found = true;
        result.entry = *entry_ptr;
        result.lba = lba;
        result.index = index;
        result.entry_ptr = entry_ptr;
    }
    
    return result;
}

FAT16_Entry* fat_find_free_dir_entry(uint32_t* lba_out) {
    uint16_t cluster = current_dir_cluster;
    
    if (cluster == FAT_ROOT_DIR_CLUSTER) {
        for (uint32_t sec = 0; sec < FAT_ROOT_DIR_SECTORS; sec++) {
            uint32_t current_lba = ROOT_DIR_START_SECTOR + sec;
            ata_read_sector(current_lba);
            
            for (int i = 0; i < 16; i++) {
                FAT16_Entry* entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));
                if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                    *lba_out = current_lba;
                    return entry;
                }
            }
        }
        return NULL;
    }
    
    uint16_t current_cluster = cluster;
    while (current_cluster < 0xFFF8) {
        uint32_t current_lba = fat_cluster_to_lba(current_cluster);
        ata_read_sector(current_lba);
        
        for (int i = 0; i < 16; i++) {
            FAT16_Entry* entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                *lba_out = current_lba;
                return entry;
            }
        }
        
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    
    println("Error: Directory full");
    return NULL;
}

// **********************************************
// ===== Deletion Operations (ИСПРАВЛЕНО!) =====
// **********************************************

void fat_delete_cluster_chain(uint16_t start_cluster) {
    if (start_cluster < 2 || start_cluster == FAT_ROOT_DIR_CLUSTER) {
        println("ERROR: Cannot delete reserved/root cluster");
        return;
    }
    
    print("DELETE: Freeing cluster chain starting at 0x");
    print_hex_byte((start_cluster >> 8) & 0xFF);
    print_hex_byte(start_cluster & 0xFF);
    print_char('\n');
    
    uint16_t current_cluster = start_cluster;
    uint32_t clusters_freed = 0;
    
    while (current_cluster >= 2 && current_cluster < 0xFFF8) {
        uint16_t next_cluster = fat_get_next_cluster(current_cluster);
        
        print("  Freeing cluster 0x");
        print_hex_byte((current_cluster >> 8) & 0xFF);
        print_hex_byte(current_cluster & 0xFF);
        print_char('\n');
        
        fat_set_entry(current_cluster, 0x0000);
        clusters_freed++;
        
        if (clusters_freed > 1000) {
            println("ERROR: Too many clusters, possible loop");
            break;
        }
        
        current_cluster = next_cluster;
    }
    
    print("SUCCESS: Freed ");
    print_hex_byte((clusters_freed >> 8) & 0xFF);
    print_hex_byte(clusters_freed & 0xFF);
    println(" clusters");
}

// КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Сохраняем данные директории!
void fat_delete_entry_data(char* name, uint8_t target_attr) {
    println("DELETE: Starting deletion process");
    
    uint32_t lba = 0;
    int index = 0;
    
    // Сохраняем информацию о файле ДО удаления цепочки
    FAT16_Entry* entry = fat_find_entry_for_modification(name, &lba, &index, target_attr);
    if (!entry) {
        println("ERROR: Entry not found");
        return;
    }
    
    // Сохраняем start_cluster и атрибуты
    uint16_t start_cluster = entry->start_cluster;
    uint8_t attributes = entry->attributes;
    
    // Проверка на . и ..
    if (strncmp(entry->name, ".          ", 11) == 0 || 
        strncmp(entry->name, "..         ", 11) == 0) {
        println("ERROR: Cannot delete . or ..");
        return;
    }
    
    // Для директорий - проверка на пустоту
    if (attributes & 0x10) {
        println("  Checking if directory is empty...");
        
        if (start_cluster < 2) {
            println("ERROR: Invalid directory cluster");
            return;
        }
        
        uint16_t cluster = start_cluster;
        bool is_empty = true;
        
        while (cluster < 0xFFF8) {
            uint32_t dir_lba = fat_cluster_to_lba(cluster);
            ata_read_sector(dir_lba);
            
            for (int i = 0; i < 16; i++) {
                FAT16_Entry* dir_entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));
                
                if (dir_entry->name[0] == 0x00) break;
                if (dir_entry->name[0] == 0xE5) continue;
                
                // Пропускаем . и ..
                if (strncmp(dir_entry->name, ".          ", 11) == 0 ||
                    strncmp(dir_entry->name, "..         ", 11) == 0) {
                    continue;
                }
                
                // Найдена непустая запись
                is_empty = false;
                println("  ERROR: Directory not empty");
                return;
            }
            
            if (!is_empty) break;
            cluster = fat_get_next_cluster(cluster);
        }
        
        println("  Directory is empty");
    }
    
    // ВАЖНО: Сохраняем сектор директории в локальный буфер
    static uint8_t dir_backup[512];
    
    // Перечитываем сектор директории
    ata_read_sector(lba);
    
    // Сохраняем в backup
    for (int i = 0; i < 512; i++) {
        dir_backup[i] = sector_buffer[i];
    }
    
    // Удаляем цепочку кластеров
    // (это изменит sector_buffer, но у нас есть backup!)
    if (start_cluster >= 2) {
        fat_delete_cluster_chain(start_cluster);
    }
    
    // Восстанавливаем сектор директории из backup
    for (int i = 0; i < 512; i++) {
        sector_buffer[i] = dir_backup[i];
    }
    
    // Теперь помечаем запись как удаленную
    println("  Marking entry as deleted");
    FAT16_Entry* entry_in_buffer = (FAT16_Entry*)(sector_buffer + index * FAT_ENTRY_SIZE);
    entry_in_buffer->name[0] = 0xE5;
    
    // Сохраняем изменения в директории
    ata_write_sector(lba, sector_buffer);
    
    println("SUCCESS: Entry deleted");
}

// **********************************************
// ===== File Creation =====
// **********************************************

void fat_write_cluster_data(uint16_t cluster, const uint8_t* data, uint32_t size, uint32_t offset) {
    if (cluster < 2 || cluster >= 0xFFF0) {
        println("ERROR: Invalid cluster");
        return;
    }
    
    uint32_t cluster_lba = fat_cluster_to_lba(cluster);
    uint32_t bytes_per_cluster = FAT_SECTORS_PER_CLUSTER * FAT_BYTES_PER_SECTOR;
    
    if (offset >= bytes_per_cluster) {
        println("ERROR: Offset exceeds cluster size");
        return;
    }
    
    uint32_t bytes_to_write = size;
    if (offset + size > bytes_per_cluster) {
        bytes_to_write = bytes_per_cluster - offset;
    }
    
    static uint8_t cluster_buffer[512];
    
    for (uint8_t sec = 0; sec < FAT_SECTORS_PER_CLUSTER; sec++) {
        uint32_t sector_offset = sec * FAT_BYTES_PER_SECTOR;
        
        ata_read_sector(cluster_lba + sec);
        
        for (int i = 0; i < FAT_BYTES_PER_SECTOR; i++) {
            cluster_buffer[i] = sector_buffer[i];
        }
        
        for (uint32_t i = 0; i < FAT_BYTES_PER_SECTOR; i++) {
            uint32_t file_pos = sector_offset + i;
            
            if (file_pos >= offset && file_pos < offset + bytes_to_write) {
                cluster_buffer[i] = data[file_pos - offset];
            }
        }
        
        for (int i = 0; i < FAT_BYTES_PER_SECTOR; i++) {
            sector_buffer[i] = cluster_buffer[i];
        }
        
        ata_write_sector(cluster_lba + sec, sector_buffer);
    }
}

bool fat_create_file(const char* name, const char* content, uint32_t size) {
    println("CREATE FILE: Starting");
    
    uint32_t cluster_size = FAT_SECTORS_PER_CLUSTER * FAT_BYTES_PER_SECTOR;
    uint32_t num_clusters_needed = (size + cluster_size - 1) / cluster_size;
    
    if (num_clusters_needed == 0) num_clusters_needed = 1;
    
    print("  Need ");
    print_hex_byte((num_clusters_needed >> 8) & 0xFF);
    print_hex_byte(num_clusters_needed & 0xFF);
    println(" clusters");
    
    uint16_t clusters[100];
    if (num_clusters_needed > 100) {
        println("ERROR: File too large");
        return false;
    }
    
    for (uint32_t i = 0; i < num_clusters_needed; i++) {
        clusters[i] = fat_find_free_cluster();
        if (clusters[i] == 0) {
            println("ERROR: Not enough free clusters");
            return false;
        }
        
        print("  Found cluster ");
        print_hex_byte((i >> 8) & 0xFF);
        print_hex_byte(i & 0xFF);
        print(": 0x");
        print_hex_byte((clusters[i] >> 8) & 0xFF);
        print_hex_byte(clusters[i] & 0xFF);
        print_char('\n');
    }
    
    println("  Building FAT chain");
    for (uint32_t i = 0; i < num_clusters_needed; i++) {
        if (i == num_clusters_needed - 1) {
            fat_set_entry(clusters[i], 0xFFFF);
        } else {
            fat_set_entry(clusters[i], clusters[i + 1]);
        }
    }
    
    println("  Writing data to clusters");
    uint32_t remaining = size;
    uint32_t written = 0;
    
    for (uint32_t i = 0; i < num_clusters_needed && remaining > 0; i++) {
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        
        fat_write_cluster_data(clusters[i], (const uint8_t*)content + written, to_write, 0);
        
        written += to_write;
        remaining -= to_write;
    }
    
    println("  Creating directory entry");
    uint32_t dir_lba = 0;
    FAT16_Entry* entry = fat_find_free_dir_entry(&dir_lba);
    
    if (!entry) {
        println("ERROR: No free directory entry, rolling back");
        fat_delete_cluster_chain(clusters[0]);
        return false;
    }
    
    fat_format_name(name, entry->name);
    entry->attributes = 0x20;
    entry->time = 0;
    entry->date = 0;
    entry->start_cluster = clusters[0];
    entry->file_size = size;
    
    ata_write_sector(dir_lba, sector_buffer);
    
    println("SUCCESS: File created");
    return true;
}

// **********************************************
// ===== Directory Creation =====
// **********************************************

void fat_create_dir(char* name) {
    println("CREATE DIR: Starting");
    
    uint16_t new_cluster = fat_find_free_cluster();
    if (new_cluster == 0) {
        println("ERROR: No free clusters");
        return;
    }
    
    fat_set_entry(new_cluster, 0xFFFF);
    
    uint8_t dir_buffer[512] = {0};
    FAT16_Entry* dot_entry = (FAT16_Entry*)dir_buffer;
    FAT16_Entry* dotdot_entry = (FAT16_Entry*)(dir_buffer + FAT_ENTRY_SIZE);
    
    fat_format_name(".", dot_entry->name);
    dot_entry->attributes = 0x10;
    dot_entry->start_cluster = new_cluster;
    
    fat_format_name("..", dotdot_entry->name);
    dotdot_entry->attributes = 0x10;
    dotdot_entry->start_cluster = (current_dir_cluster == FAT_ROOT_DIR_CLUSTER) ? 0 : current_dir_cluster;
    
    uint32_t new_dir_lba = fat_cluster_to_lba(new_cluster);
    ata_write_sector(new_dir_lba, dir_buffer);
    
    uint32_t parent_lba = 0;
    FAT16_Entry* new_entry = fat_find_free_dir_entry(&parent_lba);
    
    if (new_entry) {
        fat_format_name(name, new_entry->name);
        new_entry->attributes = 0x10;
        new_entry->time = 0;
        new_entry->date = 0;
        new_entry->start_cluster = new_cluster;
        new_entry->file_size = 0;
        
        ata_write_sector(parent_lba, sector_buffer);
        println("SUCCESS: Directory created");
    } else {
        fat_delete_cluster_chain(new_cluster);
        println("ERROR: Parent directory full");
    }
}

// **********************************************
// ===== Rename =====
// **********************************************

void fat_rename_entry(char* old_name, char* new_name) {
    uint32_t lba = 0;
    int index = 0;
    
    FAT16_Entry* entry = fat_find_entry_for_modification(old_name, &lba, &index, 0x00);
    
    if (!entry) {
        println("Error: Item not found");
        return;
    }
    
    if (fat_find_entry(new_name, 0x00).found) {
        println("Error: Name already exists");
        return;
    }
    
    fat_format_name(new_name, entry->name);
    ata_write_sector(lba, sector_buffer);
    println("Renamed successfully");
}

// **********************************************
// ===== Debug Functions =====
// **********************************************

void fat_debug_cluster_state(uint16_t cluster) {
    if (cluster < 2) {
        print("Cluster 0x");
        print_hex_byte((cluster >> 8) & 0xFF);
        print_hex_byte(cluster & 0xFF);
        println(": Reserved");
        return;
    }
    
    uint32_t fat_sector_offset = cluster / 256;
    uint16_t fat_entry_index = cluster % 256;
    uint32_t fat_sector_lba = FAT_START_SECTOR + fat_sector_offset;
    
    ata_read_sector(fat_sector_lba);
    uint16_t* fat_table = (uint16_t*)sector_buffer;
    uint16_t value = fat_table[fat_entry_index];
    
    print("Cluster 0x");
    print_hex_byte((cluster >> 8) & 0xFF);
    print_hex_byte(cluster & 0xFF);
    print(": 0x");
    print_hex_byte((value >> 8) & 0xFF);
    print_hex_byte(value & 0xFF);
    
    if (value == 0x0000) println(" (FREE)");
    else if (value == 0xFFFF) println(" (EOF)");
    else if (value >= 0xFFF8) println(" (EOC)");
    else if (value == 0xFFF7) println(" (BAD)");
    else println("");
}

void fat_debug_check_chain(uint16_t start_cluster) {
    println("DEBUG: Checking cluster chain");
    
    if (start_cluster < 2) {
        println("  ERROR: Invalid start cluster");
        return;
    }
    
    uint16_t current = start_cluster;
    uint32_t count = 0;
    
    while (current < 0xFFF8 && count < 100) {
        print("  Cluster ");
        print_hex_byte((count >> 8) & 0xFF);
        print_hex_byte(count & 0xFF);
        print(": 0x");
        print_hex_byte((current >> 8) & 0xFF);
        print_hex_byte(current & 0xFF);
        print_char('\n');
        
        current = fat_get_next_cluster(current);
        count++;
    }
    
    print("  Total: ");
    print_hex_byte((count >> 8) & 0xFF);
    print_hex_byte(count & 0xFF);
    println(" clusters");
}

uint32_t fat_get_total_sectors() {
    return fat_bpb.total_sectors_16 != 0 
        ? fat_bpb.total_sectors_16 
        : fat_bpb.total_sectors_32;
}

} // extern "C"
