#include "fat32.h"
#include "../drivers/disk.h"
#include "../lib/screen.h"
#include "../lib/string.h"
#include <stdint.h>

// ===== Глобальное состояние =====
uint32_t current_dir_cluster = FAT32_ROOT_CLUSTER;
FAT32_BPB fat32_bpb;

extern "C" {

// **********************************************
// ===== Core Helpers =====
// **********************************************

void fat32_format_name(const char* input, char* output_8_3) {
    for (int i = 0; i < 11; i++) output_8_3[i] = ' ';

    int src_i  = 0;
    int dest_i = 0;

    // Имя (до 8 символов)
    while (input[src_i] != 0 && input[src_i] != '.' && dest_i < 8) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output_8_3[dest_i++] = c;
    }

    // Расширение (до 3 символов)
    while (input[src_i] != 0 && input[src_i] != '.') src_i++;
    if (input[src_i] == '.') src_i++;
    dest_i = 8;
    while (input[src_i] != 0 && dest_i < 11) {
        char c = input[src_i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output_8_3[dest_i++] = c;
    }
}

// Кластер → LBA
// FAT32: data area начинается после зарезервированных секторов + FAT-таблицы
// Кластер 2 — первый кластер данных
uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    if (cluster < 2) return FAT32_DATA_START; // защита
    return FAT32_DATA_START + (cluster - 2) * FAT32_SECTORS_PER_CLUSTER;
}

// Читает следующий кластер из FAT32
// FAT32: 4 байта на запись, 128 записей на сектор
uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    if (current_cluster < 2) return FAT32_EOC;

    uint32_t fat_sector_lba   = FAT32_FAT_START + (current_cluster / FAT32_ENTRIES_PER_FAT_SECTOR);
    uint32_t fat_entry_index  = current_cluster % FAT32_ENTRIES_PER_FAT_SECTOR;

    ata_read_sector(fat_sector_lba);
    uint32_t* fat_table  = (uint32_t*)sector_buffer;
    uint32_t  next       = fat_table[fat_entry_index] & FAT32_MASK;

    return next;
}

// Инициализация FAT32: если FAT пустая — заполняем служебные записи
void fat32_init() {
    ata_read_sector(FAT32_FAT_START);
    uint32_t* fat_table = (uint32_t*)sector_buffer;

    if ((fat_table[0] & FAT32_MASK) == 0x00000000) {
        // FAT пустая — инициализируем
        fat_table[0] = 0x0FFFFFF8;  // Media descriptor (сохраняем верхние 4 бита)
        fat_table[1] = FAT32_EOC;   // EOC для FAT ID
        fat_table[2] = FAT32_EOC;   // Корневая директория (кластер 2) — EOF

        ata_write_sector(FAT32_FAT_START, sector_buffer);

        // Зеркало FAT
        if (FAT32_NUMBER_OF_FATS > 1) {
            ata_write_sector(FAT32_FAT_START + FAT32_SECTORS_PER_FAT, sector_buffer);
        }

        // Инициализируем кластер корневой директории (очищаем)
        uint8_t empty[FAT32_BYTES_PER_SECTOR] = {0};
        uint32_t root_lba = fat32_cluster_to_lba(FAT32_ROOT_CLUSTER);
        for (uint8_t s = 0; s < FAT32_SECTORS_PER_CLUSTER; s++) {
            ata_write_sector(root_lba + s, empty);
        }
    }

    // Устанавливаем начальную директорию — корень
    current_dir_cluster = FAT32_ROOT_CLUSTER;
}

// Устанавливает значение в FAT32 (оба экземпляра FAT)
// Важно: сохраняем старшие 4 бита записи (спецификация FAT32)
void fat32_set_entry(uint32_t cluster, uint32_t value) {
    if (cluster < 2) {
        println("FAT32 ERROR: Cannot modify reserved clusters");
        return;
    }

    uint32_t fat_sector_lba  = FAT32_FAT_START + (cluster / FAT32_ENTRIES_PER_FAT_SECTOR);
    uint32_t fat_entry_index = cluster % FAT32_ENTRIES_PER_FAT_SECTOR;

    static uint8_t fat_buf[FAT32_BYTES_PER_SECTOR];

    ata_read_sector(fat_sector_lba);
    for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) fat_buf[i] = sector_buffer[i];

    uint32_t* fat_table = (uint32_t*)fat_buf;

    // Сохраняем верхние 4 бита, пишем только нижние 28
    fat_table[fat_entry_index] = (fat_table[fat_entry_index] & 0xF0000000)
                                | (value & FAT32_MASK);

    for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) sector_buffer[i] = fat_buf[i];

    ata_write_sector(fat_sector_lba, sector_buffer);

    // Зеркало FAT
    if (FAT32_NUMBER_OF_FATS > 1) {
        ata_write_sector(fat_sector_lba + FAT32_SECTORS_PER_FAT, sector_buffer);
    }
}

// Ищет свободный кластер (возвращает 0 если нет свободных)
uint32_t fat32_find_free_cluster() {
    // Начинаем с кластера 3 (2 — корень)
    for (uint32_t cluster = 3; cluster < 65536; cluster++) {
        uint32_t fat_sector_lba  = FAT32_FAT_START + (cluster / FAT32_ENTRIES_PER_FAT_SECTOR);
        uint32_t fat_entry_index = cluster % FAT32_ENTRIES_PER_FAT_SECTOR;

        ata_read_sector(fat_sector_lba);
        uint32_t* fat_table = (uint32_t*)sector_buffer;

        if ((fat_table[fat_entry_index] & FAT32_MASK) == FAT32_FREE) {
            return cluster;
        }
    }
    return 0; // Нет свободных кластеров
}

// **********************************************
// ===== Запись данных в кластер =====
// **********************************************

void fat32_write_cluster_data(uint32_t cluster, const uint8_t* data,
                               uint32_t size, uint32_t offset) {
    if (cluster < 2) {
        println("FAT32 ERROR: Invalid cluster in write");
        return;
    }

    uint32_t cluster_lba       = fat32_cluster_to_lba(cluster);
    uint32_t bytes_per_cluster = FAT32_SECTORS_PER_CLUSTER * FAT32_BYTES_PER_SECTOR;

    if (offset >= bytes_per_cluster) {
        println("FAT32 ERROR: Offset exceeds cluster size");
        return;
    }

    uint32_t bytes_to_write = size;
    if (offset + size > bytes_per_cluster)
        bytes_to_write = bytes_per_cluster - offset;

    static uint8_t cluster_buf[FAT32_BYTES_PER_SECTOR];

    for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER; sec++) {
        uint32_t sector_offset = (uint32_t)sec * FAT32_BYTES_PER_SECTOR;

        // Проверяем, попадает ли данный сектор в диапазон записи
        if (sector_offset + FAT32_BYTES_PER_SECTOR <= offset) continue;
        if (sector_offset >= offset + bytes_to_write) continue;

        ata_read_sector(cluster_lba + sec);
        for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) cluster_buf[i] = sector_buffer[i];

        for (uint32_t i = 0; i < FAT32_BYTES_PER_SECTOR; i++) {
            uint32_t file_pos = sector_offset + i;
            if (file_pos >= offset && file_pos < offset + bytes_to_write) {
                cluster_buf[i] = data[file_pos - offset];
            }
        }

        for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) sector_buffer[i] = cluster_buf[i];
        ata_write_sector(cluster_lba + sec, sector_buffer);
    }
}

// **********************************************
// ===== Directory Operations =====
// **********************************************

// Внутренняя функция обхода директории по кластеру
// Вызывает callback для каждой записи; возвращает entry + lba + index если callback вернул true
static FAT32_DirEntry* scan_dir_cluster(uint32_t dir_cluster,
                                         const char* name_8_3,
                                         uint8_t target_attr,
                                         bool find_free,
                                         uint32_t* lba_out,
                                         int* index_out) {
    uint32_t c = dir_cluster;

    while ((c & FAT32_MASK) >= 2 && (c & FAT32_MASK) < 0x0FFFFFF8) {
        uint32_t base_lba = fat32_cluster_to_lba(c);

        for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER; sec++) {
            uint32_t lba = base_lba + sec;
            ata_read_sector(lba);

            for (int i = 0; i < FAT32_ENTRIES_PER_SECTOR; i++) {
                FAT32_DirEntry* e = (FAT32_DirEntry*)(sector_buffer + i * FAT32_ENTRY_SIZE);

                if (find_free) {
                    // Ищем свободную запись
                    if (e->name[0] == 0x00 || e->name[0] == (char)0xE5) {
                        *lba_out   = lba;
                        *index_out = i;
                        return e;
                    }
                } else {
                    // Ищем запись по имени
                    if (e->name[0] == 0x00) return NULL; // Конец каталога
                    if (e->name[0] == (char)0xE5) continue; // Удалённая запись
                    if (e->attributes == 0x0F) continue;    // LFN-запись
                    if (target_attr != 0 && !(e->attributes & target_attr)) continue;

                    if (strncmp(e->name, name_8_3, 11) == 0) {
                        *lba_out   = lba;
                        *index_out = i;
                        return e;
                    }
                }
            }
        }

        // Переходим к следующему кластеру цепочки
        uint32_t next = fat32_get_next_cluster(c);
        if ((next & FAT32_MASK) >= (FAT32_EOC & FAT32_MASK)) break;
        c = next;
    }

    return NULL;
}

FAT32_DirEntry* fat32_find_entry_for_modification(const char* name,
                                                   uint32_t* lba_out,
                                                   int* index_out,
                                                   uint8_t target_attr) {
    char name_8_3[11];

    if (strcmp(name, ".") == 0) {
        for (int i = 0; i < 11; i++) name_8_3[i] = ' ';
        name_8_3[0] = '.';
    } else if (strcmp(name, "..") == 0) {
        for (int i = 0; i < 11; i++) name_8_3[i] = ' ';
        name_8_3[0] = '.';
        name_8_3[1] = '.';
    }
    else fat32_format_name(name, name_8_3);

    return scan_dir_cluster(current_dir_cluster, name_8_3, target_attr,
                             false, lba_out, index_out);
}

FAT32_FindResult fat32_find_entry(const char* name, uint8_t target_attr) {
    FAT32_FindResult result = {0};
    uint32_t lba = 0;
    int index    = 0;

    FAT32_DirEntry* e = fat32_find_entry_for_modification(name, &lba, &index, target_attr);

    if (e) {
        result.found     = true;
        result.entry     = *e;
        result.lba       = lba;
        result.index     = index;
        result.entry_ptr = e;
    }

    return result;
}

FAT32_DirEntry* fat32_find_free_dir_entry(uint32_t* lba_out) {
    int dummy = 0;

    // Ищем свободную запись в текущей директории
    FAT32_DirEntry* e = scan_dir_cluster(current_dir_cluster, NULL, 0,
                                          true, lba_out, &dummy);
    if (e) return e;

    // Свободных нет — расширяем директорию новым кластером
    uint32_t new_cluster = fat32_find_free_cluster();
    if (new_cluster == 0) {
        println("FAT32 ERROR: No free clusters to expand directory");
        return NULL;
    }

    // Очищаем новый кластер
    uint8_t empty[FAT32_BYTES_PER_SECTOR] = {0};
    uint32_t new_lba = fat32_cluster_to_lba(new_cluster);
    for (uint8_t s = 0; s < FAT32_SECTORS_PER_CLUSTER; s++) {
        ata_write_sector(new_lba + s, empty);
    }

    // Связываем новый кластер с концом цепочки директории
    uint32_t c = current_dir_cluster;
    while (true) {
        uint32_t next = fat32_get_next_cluster(c);
        if ((next & FAT32_MASK) >= (FAT32_EOC & FAT32_MASK)) break;
        c = next;
    }
    fat32_set_entry(c, new_cluster);
    fat32_set_entry(new_cluster, FAT32_EOC);

    // Первая запись нового кластера — свободная
    ata_read_sector(new_lba);
    *lba_out = new_lba;
    return (FAT32_DirEntry*)sector_buffer;
}

// **********************************************
// ===== Deletion =====
// **********************************************

void fat32_delete_cluster_chain(uint32_t start_cluster) {
    if (start_cluster < 2) {
        println("FAT32 ERROR: Cannot delete reserved cluster");
        return;
    }

    uint32_t current = start_cluster;
    uint32_t count   = 0;

    while ((current & FAT32_MASK) >= 2 &&
           (current & FAT32_MASK) < (FAT32_EOC & FAT32_MASK)) {
        uint32_t next = fat32_get_next_cluster(current);
        fat32_set_entry(current, FAT32_FREE);
        current = next;
        count++;

        if (count > 100000) {
            println("FAT32 ERROR: Chain too long, possible loop");
            break;
        }
    }
}

void fat32_delete_entry(const char* name, uint8_t target_attr) {
    uint32_t lba = 0;
    int      idx = 0;

    FAT32_DirEntry* entry = fat32_find_entry_for_modification(name, &lba, &idx, target_attr);
    if (!entry) {
        println("FAT32 ERROR: Entry not found");
        return;
    }

    // Проверка на . и ..
    if (strncmp(entry->name, ".          ", 11) == 0 ||
        strncmp(entry->name, "..         ", 11) == 0) {
        println("FAT32 ERROR: Cannot delete . or ..");
        return;
    }

    uint32_t start_cluster = FAT32_GET_CLUSTER(entry);
    uint8_t  attributes    = entry->attributes;

    // Для директорий — проверяем пустоту
    if (attributes & 0x10) {
        uint32_t c = start_cluster;
        while ((c & FAT32_MASK) < (FAT32_EOC & FAT32_MASK)) {
            uint32_t dir_lba = fat32_cluster_to_lba(c);
            for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER; sec++) {
                ata_read_sector(dir_lba + sec);
                for (int i = 0; i < FAT32_ENTRIES_PER_SECTOR; i++) {
                    FAT32_DirEntry* de = (FAT32_DirEntry*)(sector_buffer + i * FAT32_ENTRY_SIZE);
                    if (de->name[0] == 0x00) goto done_check;
                    if (de->name[0] == (char)0xE5) continue;
                    if (strncmp(de->name, ".          ", 11) == 0 ||
                        strncmp(de->name, "..         ", 11) == 0) continue;
                    println("FAT32 ERROR: Directory not empty");
                    return;
                }
            }
            c = fat32_get_next_cluster(c);
        }
        done_check:;
    }

    // Сохраняем сектор директории (fat32_delete_cluster_chain изменит sector_buffer)
    static uint8_t dir_backup[FAT32_BYTES_PER_SECTOR];
    ata_read_sector(lba);
    for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) dir_backup[i] = sector_buffer[i];

    // Удаляем цепочку кластеров
    if (start_cluster >= 2) fat32_delete_cluster_chain(start_cluster);

    // Восстанавливаем сектор и помечаем запись удалённой
    for (int i = 0; i < FAT32_BYTES_PER_SECTOR; i++) sector_buffer[i] = dir_backup[i];

    FAT32_DirEntry* e_in_buf = (FAT32_DirEntry*)(sector_buffer + idx * FAT32_ENTRY_SIZE);
    e_in_buf->name[0] = (char)0xE5;

    ata_write_sector(lba, sector_buffer);
    println("FAT32: Entry deleted");
}

// **********************************************
// ===== File Creation =====
// **********************************************

bool fat32_create_file(const char* name, const char* content, uint32_t size) {
    uint32_t cluster_size       = FAT32_SECTORS_PER_CLUSTER * FAT32_BYTES_PER_SECTOR;
    uint32_t num_clusters       = (size + cluster_size - 1) / cluster_size;
    if (num_clusters == 0) num_clusters = 1;

    if (num_clusters > 256) {
        println("FAT32 ERROR: File too large");
        return false;
    }

    uint32_t clusters[256];
    for (uint32_t i = 0; i < num_clusters; i++) {
        clusters[i] = fat32_find_free_cluster();
        if (clusters[i] == 0) {
            println("FAT32 ERROR: Not enough free clusters");
            return false;
        }
        // Временно помечаем как занятый чтобы find_free не вернул тот же кластер
        fat32_set_entry(clusters[i], FAT32_EOC);
    }

    // Строим цепочку FAT
    for (uint32_t i = 0; i < num_clusters - 1; i++) {
        fat32_set_entry(clusters[i], clusters[i + 1]);
    }
    fat32_set_entry(clusters[num_clusters - 1], FAT32_EOC);

    // Пишем данные
    uint32_t remaining = size;
    uint32_t written   = 0;
    for (uint32_t i = 0; i < num_clusters && remaining > 0; i++) {
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        fat32_write_cluster_data(clusters[i], (const uint8_t*)content + written, to_write, 0);
        written   += to_write;
        remaining -= to_write;
    }

    // Создаём запись директории
    uint32_t dir_lba = 0;
    FAT32_DirEntry* entry = fat32_find_free_dir_entry(&dir_lba);
    if (!entry) {
        println("FAT32 ERROR: No free directory entry, rolling back");
        for (uint32_t i = 0; i < num_clusters; i++) fat32_set_entry(clusters[i], FAT32_FREE);
        return false;
    }

    fat32_format_name(name, entry->name);
    entry->attributes   = 0x20; // ARCHIVE
    entry->nt_reserved  = 0;
    entry->crt_time_tenth = 0;
    entry->crt_time     = 0;
    entry->crt_date     = 0;
    entry->lst_acc_date = 0;
    entry->wrt_time     = 0;
    entry->wrt_date     = 0;
    entry->file_size    = size;
    FAT32_SET_CLUSTER(entry, clusters[0]);

    ata_write_sector(dir_lba, sector_buffer);
    println("FAT32: File created");
    return true;
}

// **********************************************
// ===== Directory Creation =====
// **********************************************

void fat32_create_dir(char* name) {
    uint32_t new_cluster = fat32_find_free_cluster();
    if (new_cluster == 0) {
        println("FAT32 ERROR: No free clusters");
        return;
    }
    fat32_set_entry(new_cluster, FAT32_EOC);

    // Инициализируем кластер новой директории
    uint8_t dir_buf[FAT32_BYTES_PER_SECTOR] = {0};
    FAT32_DirEntry* dot    = (FAT32_DirEntry*)dir_buf;
    FAT32_DirEntry* dotdot = (FAT32_DirEntry*)(dir_buf + FAT32_ENTRY_SIZE);

    // Запись "."
    for (int i = 0; i < 8; i++) dot->name[i] = ' ';
    for (int i = 0; i < 3; i++) dot->ext[i]  = ' ';
    dot->name[0] = '.';
    dot->attributes = 0x10;
    FAT32_SET_CLUSTER(dot, new_cluster);

    // Запись ".."
    for (int i = 0; i < 8; i++) dotdot->name[i] = ' ';
    for (int i = 0; i < 3; i++) dotdot->ext[i]  = ' ';
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    dotdot->attributes = 0x10;
    FAT32_SET_CLUSTER(dotdot, current_dir_cluster);

    uint32_t new_dir_lba = fat32_cluster_to_lba(new_cluster);
    ata_write_sector(new_dir_lba, dir_buf);

    // Очищаем оставшиеся секторы кластера
    uint8_t empty[FAT32_BYTES_PER_SECTOR] = {0};
    for (uint8_t s = 1; s < FAT32_SECTORS_PER_CLUSTER; s++) {
        ata_write_sector(new_dir_lba + s, empty);
    }

    // Добавляем запись в родительскую директорию
    uint32_t parent_lba = 0;
    FAT32_DirEntry* new_entry = fat32_find_free_dir_entry(&parent_lba);

    if (!new_entry) {
        fat32_delete_cluster_chain(new_cluster);
        println("FAT32 ERROR: Parent directory full");
        return;
    }

    fat32_format_name(name, new_entry->name);
    new_entry->attributes = 0x10; // DIRECTORY
    new_entry->nt_reserved = 0;
    new_entry->crt_time_tenth = 0;
    new_entry->crt_time = 0;
    new_entry->crt_date = 0;
    new_entry->lst_acc_date = 0;
    new_entry->wrt_time = 0;
    new_entry->wrt_date = 0;
    new_entry->file_size = 0;
    FAT32_SET_CLUSTER(new_entry, new_cluster);

    ata_write_sector(parent_lba, sector_buffer);
    println("FAT32: Directory created");
}

// **********************************************
// ===== Rename =====
// **********************************************

void fat32_rename_entry(char* old_name, char* new_name) {
    uint32_t lba = 0;
    int      idx = 0;

    FAT32_DirEntry* entry = fat32_find_entry_for_modification(old_name, &lba, &idx, 0x00);
    if (!entry) {
        println("FAT32 ERROR: Item not found");
        return;
    }

    if (fat32_find_entry(new_name, 0x00).found) {
        println("FAT32 ERROR: Name already exists");
        return;
    }

    fat32_format_name(new_name, entry->name);
    ata_write_sector(lba, sector_buffer);
    println("FAT32: Renamed successfully");
}

// **********************************************
// ===== Debug =====
// **********************************************

void fat32_debug_cluster_state(uint32_t cluster) {
    if (cluster < 2) { println("FAT32 DEBUG: Reserved cluster"); return; }

    uint32_t fat_sector_lba  = FAT32_FAT_START + (cluster / FAT32_ENTRIES_PER_FAT_SECTOR);
    uint32_t fat_entry_index = cluster % FAT32_ENTRIES_PER_FAT_SECTOR;

    ata_read_sector(fat_sector_lba);
    uint32_t* fat_table = (uint32_t*)sector_buffer;
    uint32_t  value     = fat_table[fat_entry_index] & FAT32_MASK;

    print("Cluster 0x");
    print_hex_byte((cluster >> 24) & 0xFF);
    print_hex_byte((cluster >> 16) & 0xFF);
    print_hex_byte((cluster >> 8)  & 0xFF);
    print_hex_byte(cluster         & 0xFF);
    print(": 0x");
    print_hex_byte((value >> 24) & 0xFF);
    print_hex_byte((value >> 16) & 0xFF);
    print_hex_byte((value >> 8)  & 0xFF);
    print_hex_byte(value         & 0xFF);

    if (value == FAT32_FREE) println(" (FREE)");
    else if ((value & FAT32_MASK) >= (FAT32_EOC & FAT32_MASK)) println(" (EOC/EOF)");
    else if ((value & FAT32_MASK) == (FAT32_BAD & FAT32_MASK)) println(" (BAD)");
    else println("");
}

void fat32_debug_check_chain(uint32_t start_cluster) {
    println("FAT32 DEBUG: Checking cluster chain");
    uint32_t current = start_cluster;
    uint32_t count   = 0;

    while ((current & FAT32_MASK) >= 2 &&
           (current & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) &&
           count < 200) {
        current = fat32_get_next_cluster(current);
        count++;
    }

    print("  Total clusters: ");
    print_hex_byte((count >> 8) & 0xFF);
    print_hex_byte(count & 0xFF);
    print_char('\n');
}

// Возвращает список имён в текущей директории, начинающихся на prefix.
// results — массив строк, max — максимум совпадений.
// Возвращает количество найденных.
int fat32_tab_complete(const char* prefix, char results[][128], int max) {
    int prefix_len = 0;
    while (prefix[prefix_len]) prefix_len++;

    int count = 0;
    uint32_t c = current_dir_cluster;

    while ((c & FAT32_MASK) >= 2 && (c & FAT32_MASK) < 0x0FFFFFF8 && count < max) {
        uint32_t base_lba = fat32_cluster_to_lba(c);
        for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER && count < max; sec++) {
            ata_read_sector(base_lba + sec);
            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buffer;
            for (int i = 0; i < 16 && count < max; i++) {
                uint8_t first = (uint8_t)entries[i].name[0];
                if (first == 0x00) goto done;
                if (first == 0xE5) continue;
                if (entries[i].attributes == 0x0F) continue; // LFN
                if (entries[i].name[0] == '.') continue;     // . и ..

                // Декодируем имя из 8.3 в строку
                char decoded[13] = {0};
                int di = 0;
                for (int n = 0; n < 8 && entries[i].name[n] != ' '; n++)
                    decoded[di++] = entries[i].name[n] | 32; // в нижний регистр
                bool has_ext = false;
                for (int n = 0; n < 3; n++) if (entries[i].ext[n] != ' ') { has_ext = true; break; }
                if (has_ext) {
                    decoded[di++] = '.';
                    for (int n = 0; n < 3 && entries[i].ext[n] != ' '; n++)
                        decoded[di++] = entries[i].ext[n] | 32;
                }
                decoded[di] = 0;

                // Проверяем совпадение с префиксом
                bool match = true;
                for (int n = 0; n < prefix_len; n++) {
                    char a = prefix[n], b = decoded[n];
                    if (a >= 'A' && a <= 'Z') a += 32;
                    if (b >= 'A' && b <= 'Z') b += 32;
                    if (!b || a != b) { match = false; break; }
                }
                if (match) {
                    int k = 0;
                    while (decoded[k]) { results[count][k] = decoded[k]; k++; }
                    results[count][k] = 0;
                    count++;
                }
            }
        }
        c = fat32_get_next_cluster(c);
    }
done:
    return count;
}

} // extern "C"