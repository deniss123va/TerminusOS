#include "config.h"
#include "../drivers/fat32.h"
#include "../lib/screen.h"
#include "../lib/string.h"
#include "../drivers/disk.h"

// Буфер для чтения конфига (один сектор)
static char config_buffer[512];

void parse_line(char* line) {
    // Ищем разделитель '='
    char* equals = 0;
    int len = strlen(line);
    
    for(int i=0; i<len; i++) {
        if(line[i] == '=') {
            equals = &line[i];
            break;
        }
    }
    
    if (!equals) return;
    
    // Разделяем строку на ключ и значение
    *equals = 0;
    char* key = line;
    char* value = equals + 1;
    
    // Очистка от \r (если файл создан в Windows)
    int val_len = strlen(value);
    if (val_len > 0 && value[val_len-1] == '\r') {
        value[val_len-1] = 0;
    }

    // === ОБРАБОТКА НАСТРОЕК ===
    
    if (strcmp(key, "THEME") == 0) {
        set_theme(value);
    }
    
    // Сюда можно добавлять другие настройки в будущем
    // else if (strcmp(key, "KEYBOARD") == 0) { ... }
}

void create_default_config() {
    const char* default_cfg = "THEME=default\n";
    fat32_create_file("boot.cfg", default_cfg, strlen(default_cfg));
    println("Config: Created default boot.cfg");
}

void config_load() {
    //println("Config: Loading boot.cfg...");
    
    FAT32_FindResult res = fat32_find_entry("boot.cfg", 0x00);
    
    if (!res.found) {
        println("Config: File not found.");
        create_default_config();
        return;
    }
    
    uint32_t cluster = FAT32_GET_CLUSTER(&res.entry);
    uint32_t size = res.entry.file_size;
    
    if (size == 0 || size > 512) {
        //println("Config: Invalid size (max 512 bytes).");
        return;
    }
    
    // Читаем первый сектор файла
    uint32_t lba = fat32_cluster_to_lba(cluster);
    ata_read_sector(lba);
    
    // Копируем в буфер для обработки
    for(int i=0; i<512; i++) config_buffer[i] = sector_buffer[i];
    
    // Парсим построчно
    char line[64];
    int line_idx = 0;
    
    for(int i=0; i < size; i++) {
        char c = config_buffer[i];
        
        if (c == '\n' || c == 0) {
            line[line_idx] = 0;
            if (line_idx > 0) {
                parse_line(line);
            }
            line_idx = 0;
        } else {
            if (line_idx < 63) {
                line[line_idx++] = c;
            }
        }
    }
    
    // Обработка последней строки, если нет \n в конце
    if (line_idx > 0) {
        line[line_idx] = 0;
        parse_line(line);
    }
    
    //println("Config: Loaded.");
}