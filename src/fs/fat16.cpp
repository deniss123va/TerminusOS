#include "fat16.h"
#include "../drivers/disk.h" // Для ata_read_sector, sector_buffer
#include "../kernel/screen.h" // Для println
#include "../lib/string.h" // Для strcpy, strcmp, strncmp

// Глобальное состояние FAT16
uint16_t current_dir_cluster = 0; 

// **********************************************
// ===== 6. FAT16 Core Helpers (Кластеры и имена) =====
// **********************************************

// Форматирование имени в формат 8.3 в верхнем регистре
void fat_format_name(const char* input, char* output_8_3) {
    for (int i = 0; i < 11; i++) {
        output_8_3[i] = ' ';
    }
    
    int src_i = 0;
    int dest_i = 0;
    
    // Имя (до 8 символов)
    while (input[src_i] != 0 && input[src_i] != '.' && dest_i < 8) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32; 
        output_8_3[dest_i++] = c;
    }
    
    // Переход к расширению
    while (input[src_i] != 0 && input[src_i] != '.') src_i++;
    if (input[src_i] == '.') src_i++;

    // Расширение (до 3 символов)
    dest_i = 8;
    while (input[src_i] != 0 && dest_i < 11) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32; 
        output_8_3[dest_i++] = c;
    }
}

// Вычисляет LBA первого сектора кластера данных
uint32_t fat_cluster_to_lba(uint16_t cluster) {
    if (cluster == FAT_ROOT_DIR_CLUSTER) {
        // Доступ к корневой директории
        return ROOT_DIR_START_SECTOR;
    }
    // Кластеры данных нумеруются с 2
    return DATA_START_SECTOR + (cluster - 2) * FAT_SECTORS_PER_CLUSTER;
}

// Находит следующий кластер в цепочке FAT
uint16_t fat_get_next_cluster(uint16_t current_cluster) {
    if (current_cluster < 2 || current_cluster >= 0xFFF0) return 0xFFFF;

    // Читаем сектор FAT
    uint32_t fat_sector_lba = FAT_START_SECTOR + (current_cluster / (FAT_BYTES_PER_SECTOR / 2));
    uint16_t fat_entry_offset = current_cluster % (FAT_BYTES_PER_SECTOR / 2);
    
    ata_read_sector(fat_sector_lba);
    uint16_t next_cluster = ((uint16_t*)sector_buffer)[fat_entry_offset];
    
    return next_cluster;
}

// Находит свободный кластер, помечает его и возвращает его номер
uint16_t fat_find_free_cluster_and_mark(uint16_t mark_val) {
    // В минимальной реализации ищем только в первом секторе FAT
    ata_read_sector(FAT_START_SECTOR);
    uint16_t* fat_table = (uint16_t*)sector_buffer;

    for (uint16_t cluster = 2; cluster < 256; cluster++) { 
        if (fat_table[cluster] == 0x0000) { 
            // Помечаем кластер
            fat_table[cluster] = mark_val; 
            // Записываем обратно в обе таблицы FAT
            ata_write_sector(FAT_START_SECTOR, sector_buffer);
            ata_write_sector(FAT_START_SECTOR + FAT_SECTORS_PER_FAT, sector_buffer);
            return cluster;
        }
    }
    println("Error: No free clusters found!");
    return 0; 
}


// **********************************************
// ===== 7. FAT16 Directory Operations (Навигация) =====
// **********************************************

// Ищет запись по имени в заданной директории (кластер current_dir_cluster)
// Возвращает структуру FAT_FindResult (для чтения)
FAT_FindResult fat_find_entry(const char* name, uint8_t target_attr) {
    FAT_FindResult result = {0};
    // Использование fat_find_entry_for_modification для выполнения поиска
    uint32_t lba = 0;
    int index = 0;
    
    FAT16_Entry* entry_ptr = fat_find_entry_for_modification(name, &lba, &index, target_attr);
    
    if (entry_ptr != 0) {
        result.found = true;
        result.entry = *entry_ptr; // Копируем найденную запись
        result.lba = lba;
        result.index = index;
    }
    
    return result;
}


// Ищет запись по имени в заданной директории (кластер current_dir_cluster)
// Возвращает УКАЗАТЕЛЬ на запись в sector_buffer (для модификации)
// Заполняет lba_out и index_out
FAT16_Entry* fat_find_entry_for_modification(const char* name, uint32_t* lba_out, int* index_out, uint8_t target_attr) {
    char name_8_3[11];
    
    // Форматируем имя (с учётом . и ..)
    if (strcmp(name, ".") == 0) {
        strcpy(name_8_3, ".          ");
    } else if (strcmp(name, "..") == 0) {
        strcpy(name_8_3, "..         ");
    } else {
        fat_format_name(name, name_8_3);
    }

    uint16_t cluster = current_dir_cluster;

    // === КОРНЕВАЯ ДИРЕКТОРИЯ ===
    if (cluster == FAT_ROOT_DIR_CLUSTER) {
        for (uint32_t sec = 0; sec < FAT_ROOT_DIR_SECTORS; sec++) {
            uint32_t lba = ROOT_DIR_START_SECTOR + sec;
            ata_read_sector(lba);

            for (int i = 0; i < 16; i++) {
                FAT16_Entry* e = (FAT16_Entry*)(sector_buffer + i * FAT_ENTRY_SIZE);

                if (e->name[0] == 0x00) return 0; // Найдена конечная запись
                if (e->name[0] == 0xE5 || e->attributes == 0x0F) continue; // Удалено или LFN
                if (target_attr != 0 && !(e->attributes & target_attr)) continue; // Проверяем атрибут

                if (strncmp(e->name, name_8_3, 8) == 0 &&
                    strncmp(e->ext,  name_8_3 + 8, 3) == 0)
                {
                    *lba_out = lba;
                    *index_out = i;
                    return e;
                }
            }
        }
        return 0;
    }

    // === ПОДКАТАЛОГИ ===
    uint16_t c = cluster;
    while (c < 0xFFF8) {
        uint32_t lba = fat_cluster_to_lba(c);
        ata_read_sector(lba);

        for (int i = 0; i < 16; i++) {
            FAT16_Entry* e = (FAT16_Entry*)(sector_buffer + i * FAT_ENTRY_SIZE);

            if (e->name[0] == 0x00) return 0; // Найдена конечная запись
            if (e->name[0] == 0xE5 || e->attributes == 0x0F) continue; // Удалено или LFN
            if (target_attr != 0 && !(e->attributes & target_attr)) continue; // Проверяем атрибут

            if (strncmp(e->name, name_8_3, 8) == 0 &&
                strncmp(e->ext,  name_8_3 + 8, 3) == 0)
            {
                *lba_out = lba;
                *index_out = i;
                return e;
            }
        }

        c = fat_get_next_cluster(c);
    }

    return 0;
}


// Ищет свободный слот в текущей директории
FAT16_Entry* fat_find_free_dir_entry(uint32_t* lba_out) {
    uint16_t cluster = current_dir_cluster;

    // --- 1. Корневая директория ---
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
        return 0; // Корневая директория заполнена
    }

    // --- 2. Кластерная директория ---
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
        // Переход к следующему кластеру (не реализовано выделение нового кластера при заполнении)
        current_cluster = fat_get_next_cluster(current_cluster);
    }
    println("Error: Directory is full and chain expansion is not supported.");
    return 0; 
}


// Удаляет цепочку кластеров из FAT-таблиц
void fat_delete_cluster_chain(uint16_t start_cluster) {
    if (start_cluster < 2) {
        // Дополнительная проверка на рут и зарезервированные кластеры
        if (start_cluster == FAT_ROOT_DIR_CLUSTER) {
            println("Error: Cannot delete the root directory cluster chain.");
        }
        return;
    }
    
    uint16_t current = start_cluster;
    uint16_t next;

    while (current < 0xFFF8) {
        // Проверка на корректность кластера, чтобы не выйти за пределы FAT
        if (current >= 0xFFF0) break; 
        
        // Читаем FAT сектор
        uint32_t fat_sector_lba = FAT_START_SECTOR + (current / (FAT_BYTES_PER_SECTOR / 2));
        uint16_t fat_entry_offset = current % (FAT_BYTES_PER_SECTOR / 2);
        
        // ВНИМАНИЕ: Для удаления нужно прочитать СЕКТОР FAT, а не только кластер.
        // Вы читаете весь сектор (512 байт / 2 байта на запись = 256 записей FAT на сектор).
        
        ata_read_sector(fat_sector_lba);
        uint16_t* fat_table = (uint16_t*)sector_buffer;

        next = fat_table[fat_entry_offset];
        fat_table[fat_entry_offset] = 0x0000; // Помечаем как свободный

        // Записываем обратно в обе таблицы FAT
        ata_write_sector(fat_sector_lba, sector_buffer); // Используем fat_sector_lba, а не FAT_START_SECTOR
        ata_write_sector(fat_sector_lba + FAT_SECTORS_PER_FAT, sector_buffer);
        
        current = next;
    }
}

// **********************************************
// ===== 8. FAT16 File/Directory Modification Logic =====
// **********************************************

// Создает файл (использует текущую директорию)
bool fat_create_file(const char* name, const char* content, uint32_t size) {
    
    uint16_t start_cluster = fat_find_free_cluster_and_mark(0xFFFF); // 0xFFFF - конец цепочки
    if (start_cluster == 0) return false;

    // Вычисляем LBA сектора данных
    uint32_t data_lba = fat_cluster_to_lba(start_cluster);
    
    // Записываем данные файла (ограничено 512 байтами)
    uint8_t write_buffer[512] = {0};
    for (uint32_t i = 0; i < size && i < 512; i++) {
        write_buffer[i] = content[i];
    }
    ata_write_sector(data_lba, write_buffer);

    // Ищем свободное место в текущей директории
    uint32_t dir_lba = 0;
    FAT16_Entry* entry = fat_find_free_dir_entry(&dir_lba);
    
    if (entry) {
        fat_format_name(name, entry->name);
        entry->attributes = 0x20; // Archive
        entry->time = 0;
        entry->date = 0;
        entry->start_cluster = start_cluster;
        entry->file_size = size;

        // Здесь sector_buffer содержит данные текущей директории с новой записью
        ata_write_sector(dir_lba, sector_buffer); 
        
        println("File created and saved successfully.");
        return true;
    }
    
    // Если не удалось создать запись, очищаем кластер
    fat_delete_cluster_chain(start_cluster);
    println("Error: Current Directory is full.");
    return false;
}

// Создает новую директорию
void fat_create_dir(char* name) {
    // 1. Найти свободный кластер для новой директории
    uint16_t new_cluster = fat_find_free_cluster_and_mark(0xFFFF);
    if (new_cluster == 0) return;

    uint32_t new_dir_lba = fat_cluster_to_lba(new_cluster);
    
    // 2. Инициализация кластера новой директории (. и ..)
    uint8_t dir_buffer[512] = {0};
    FAT16_Entry* dot_entry = (FAT16_Entry*)dir_buffer;
    FAT16_Entry* dotdot_entry = (FAT16_Entry*)(dir_buffer + FAT_ENTRY_SIZE);

    // Запись '.' (текущая директория)
    fat_format_name(".", dot_entry->name); // Используем fat_format_name для корректного 8.3
    dot_entry->attributes = 0x10;
    dot_entry->start_cluster = new_cluster;
    
    // Запись '..' (родительская директория)
    fat_format_name("..", dotdot_entry->name); // Используем fat_format_name для корректного 8.3
    dotdot_entry->attributes = 0x10;
    // Родитель - это текущий кластер (если root, то 0)
    dotdot_entry->start_cluster = (current_dir_cluster == FAT_ROOT_DIR_CLUSTER) ? 0 : current_dir_cluster;

    ata_write_sector(new_dir_lba, dir_buffer);

    // 3. Создание записи в текущей директории
    uint32_t parent_lba = 0;
    FAT16_Entry* new_entry = fat_find_free_dir_entry(&parent_lba);
    
    if (new_entry) {
        fat_format_name(name, new_entry->name);
        new_entry->attributes = 0x10; // Directory
        new_entry->time = 0;
        new_entry->date = 0;
        new_entry->start_cluster = new_cluster;
        new_entry->file_size = 0;

        // Здесь sector_buffer содержит данные родительской директории с новой записью
        ata_write_sector(parent_lba, sector_buffer); 
        println("Directory created successfully.");
    } else {
        // Сбой создания записи: откатываем кластер
        fat_delete_cluster_chain(new_cluster);
        println("Error: Failed to create directory entry in parent directory.");
    }
}


// Удаляет файл или пустую директорию
void fat_delete_entry_data(char* name, uint8_t target_attr) {
    uint32_t lba = 0;
    int index = 0;
    
    // Ищем запись для МОДИФИКАЦИИ
    FAT16_Entry* entry = fat_find_entry_for_modification(name, &lba, &index, target_attr);
    
    if (!entry) {
        println("Error: File or Directory not found.");
        return;
    }

    // Дополнительная проверка на "." и ".."
    if (strncmp(entry->name, ".          ", 11) == 0 || strncmp(entry->name, "..         ", 11) == 0) {
        println("Error: Cannot delete '.' or '..' entries.");
        return;
    }
    
    // 1. Проверяем, что директория пуста (только . и ..)
    if (entry->attributes & 0x10) { 
        // НЕЛЬЗЯ УДАЛЯТЬ КОРНЕВУЮ ДИРЕКТОРИЮ (Cluster 0)
        if (entry->start_cluster == FAT_ROOT_DIR_CLUSTER) {
            println("Error: Cannot delete the root directory.");
            return;
        }

        uint32_t dir_lba = fat_cluster_to_lba(entry->start_cluster);
        
        bool is_empty = true;
        // Проверяем все сектора кластерной цепочки
        uint16_t c = entry->start_cluster;
        while(c < 0xFFF8) {
            dir_lba = fat_cluster_to_lba(c);
            ata_read_sector(dir_lba);
            
            // Начинаем с 2, чтобы пропустить '.' и '..'
            for (int i = 2; i < 16; i++) { 
                FAT16_Entry* inner_entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));
                if (inner_entry->name[0] != 0x00 && inner_entry->name[0] != 0xE5) {
                    is_empty = false;
                    break;
                }
            }
            if (!is_empty) break; // Выходим из обоих циклов
            
            c = fat_get_next_cluster(c);
        }
        
        if (!is_empty) {
            println("Error: Directory is not empty.");
            return;
        }
    }
    
    // 2. Освобождаем кластеры в FAT
    fat_delete_cluster_chain(entry->start_cluster);
    
    // 3. Помечаем запись в директории как удаленную (0xE5)
    entry->name[0] = 0xE5;
    ata_write_sector(lba, sector_buffer);
    
    println("Item deleted successfully.");
}

// Переименование файла/папки
void fat_rename_entry(char* old_name, char* new_name) {
    uint32_t lba = 0;
    int index = 0;
    
    // Ищем старую запись (атрибут 0x00 - ищем файл или папку) для МОДИФИКАЦИИ
    FAT16_Entry* entry = fat_find_entry_for_modification(old_name, &lba, &index, 0x00); 
    
    if (!entry) {
        println("Error: Item not found.");
        return;
    }

    // Проверяем, не существует ли уже файл с новым именем
    // Используем новую, безопасную функцию поиска
    if (fat_find_entry(new_name, 0x00).found) {
        println("Error: Destination name already exists.");
        return;
    }
    
    // 1. Форматируем новое имя 8.3 (FAT16 позволяет переименование простой сменой имени)
    fat_format_name(new_name, entry->name);
    
    // 2. Записываем сектор обратно
    ata_write_sector(lba, sector_buffer);
    
    println("Item renamed successfully.");
}