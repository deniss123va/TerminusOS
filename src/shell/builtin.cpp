#include "builtin.h"
#include "../kernel/screen.h" 
#include "../kernel/keyboard.h" 
#include "../drivers/disk.h" 
#include "../fs/vfs.h" 
#include "../fs/fat16.h" 
#include "../lib/string.h" 
#include "../lib/utils.h" 
#include "shell.h" 
#include "../drivers/commands/cmd_info.h"
#include "../drivers/commands/cmd_nano.h"

// **********************************************
// ===== Shell Commands (Определения команд) =====
// **********************************************

// Выводит текущую директорию (человекопонятный путь)
void cmd_pwd() {
    println(current_path);
}

// Смена директории (с обновлением пути)
void cmd_cd(char* name) {
    if (strcmp(name, "/") == 0) {
        current_dir_cluster = FAT_ROOT_DIR_CLUSTER;
        strcpy(current_path, "/"); 
        println("Changed to /");
        return;
    }
    
    if (strcmp(name, ".") == 0) {
        println("Directory remains the same.");
        return;
    }
    
    // ИСПРАВЛЕНИЕ: Используем новую структуру FAT_FindResult
    FAT_FindResult result = fat_find_entry(name, 0x10); // Ищем только директории (0x10)

    if (!result.found) {
        println("Error: Directory not found or is a file.");
        return;
    }

    uint16_t target_cluster = result.entry.start_cluster;

    // --- ОБНОВЛЕНИЕ ПУТИ ---
    if (strcmp(name, "..") == 0) {
        if (current_dir_cluster != FAT_ROOT_DIR_CLUSTER) {
            dirname(current_path); 
        }
        if (target_cluster == 0) { 
            strcpy(current_path, "/");
        }
    } else {
        if (strcmp(current_path, "/") != 0) {
            strcat(current_path, "/");
        }
        strcat(current_path, name);
    }
    
    // --- СМЕНА КЛАСТЕРА ---
    if (target_cluster == 0) {
        current_dir_cluster = FAT_ROOT_DIR_CLUSTER;
    } else {
        current_dir_cluster = target_cluster;
    }
    
    println("Changed directory.");
}


// Создание директории
void cmd_mkdir(char* name) {
    if (strlen(name) == 0) {
        println("Usage: mkdir <directory_name>");
        return;
    }
    
    // ИСПРАВЛЕНИЕ: Используем новую функцию поиска
    if (fat_find_entry(name, 0x00).found) { 
        println("Error: Item already exists.");
        return;
    }
    
    fat_create_dir(name);
}

// Удаление файла или пустой директории
void cmd_rm(char* name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "/") == 0) {
        println("Error: Cannot remove special directories.");
        return;
    }
    // Вызов функции удаления, которая теперь имеет внутренние защиты от удаления Root-директории и спец.записей.
    fat_delete_entry_data(name, 0x00);
}

// Переименование файла или папки
void cmd_mv(char* args) {
    char old_name[128] = {0};
    char new_name[128] = {0};
    
    int len = strlen(args);
    int space_pos = 0;
    while(space_pos < len && args[space_pos] != ' ') space_pos++;

    if (space_pos == len || space_pos == 0) {
        println("Usage: mv <old_name> <new_name>");
        return;
    }

    for(int i = 0; i < space_pos; i++) old_name[i] = args[i];
    old_name[space_pos] = 0;

    int new_start = space_pos + 1;
    for(int i = new_start; i < len; i++) new_name[i - new_start] = args[i];
    
    if (strlen(new_name) == 0) {
        println("Usage: mv <old_name> <new_name>");
        return;
    }

    fat_rename_entry(old_name, new_name);
}


// Листинг содержимого текущей директории
void cmd_ls_disk() {
    println("--- Disk Directory (FAT16) ---");
    
    uint16_t cluster = current_dir_cluster;

    if (cluster == FAT_ROOT_DIR_CLUSTER) {
        for (uint32_t sec = 0; sec < FAT_ROOT_DIR_SECTORS; sec++) {
            uint32_t current_lba = ROOT_DIR_START_SECTOR + sec;
            ata_read_sector(current_lba);

            for (int i = 0; i < 16; i++) {
                FAT16_Entry* entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));

                if (entry->name[0] == 0x00) goto ls_end; 
                if (entry->name[0] == 0xE5 || entry->attributes == 0x0F || entry->attributes == 0x08) continue; 
                
                for (int j = 0; j < 8; j++) if (entry->name[j] != ' ') print_char(entry->name[j]);
                if (!(entry->attributes & 0x10)) { 
                    print_char('.');
                    for (int j = 0; j < 3; j++) if (entry->ext[j] != ' ') print_char(entry->ext[j]);
                }
                
                if (entry->attributes & 0x10) print(" <DIR>");
                print_char('\n');
            }
        }
    }
    else {
        uint16_t current_cluster = cluster;
        while (current_cluster < 0xFFF8) {
            uint32_t current_lba = fat_cluster_to_lba(current_cluster);
            ata_read_sector(current_lba);

            for (int i = 0; i < 16; i++) {
                FAT16_Entry* entry = (FAT16_Entry*)(sector_buffer + (i * FAT_ENTRY_SIZE));

                if (entry->name[0] == 0x00) goto ls_end; 
                if (entry->name[0] == 0xE5 || entry->attributes == 0x0F || entry->attributes == 0x08) continue; 
                
                for (int j = 0; j < 8; j++) if (entry->name[j] != ' ') print_char(entry->name[j]);
                if (!(entry->attributes & 0x10)) {
                    print_char('.');
                    for (int j = 0; j < 3; j++) if (entry->ext[j] != ' ') print_char(entry->ext[j]);
                }
                
                if (entry->attributes & 0x10) print(" <DIR>");
                print_char('\n');
            }
            current_cluster = fat_get_next_cluster(current_cluster);
        }
    }

ls_end:
    println("--- End of Directory ---");
}


// Читает файл с диска (FAT16) и печатает его содержимое (только первый сектор)
void cmd_disk_cat(char* name) {
    
    // ИСПРАВЛЕНИЕ: Используем новую структуру FAT_FindResult
    FAT_FindResult result = fat_find_entry(name, 0x00);

    if (!result.found || (result.entry.attributes & 0x10)) { 
        println("Error: File not found or is a directory.");
        return;
    }

    uint16_t start_cluster = result.entry.start_cluster;
    uint32_t file_size = result.entry.file_size;

    if (start_cluster < 2) {
        println("Error: Invalid cluster.");
        return;
    }

    uint32_t data_lba = fat_cluster_to_lba(start_cluster);
    
    ata_read_sector(data_lba);
    
    // 3. Вывод содержимого
    println("--- File Content ---");
    for (uint32_t i = 0; i < file_size && i < 512; i++) {
        print_char(sector_buffer[i]);
    }
    print_char('\n'); 
    println("--- End of File ---");
}

void cmd_ls_vfs() {
    for(int i=0;i<MAX_FILES;i++){
        println(fs[i].name);
    }
}

void cmd_cat(char* name){
    for(int i=0;i<MAX_FILES;i++){
        if(strcmp(fs[i].name,name)==0){
            println(fs[i].content);
            return;
        }
    }
    println("File not found");
}

void cmd_read_disk() {
    println("Reading Sector 0 (Boot Sector)...");
    ata_read_sector(0);
    
    println("First 32 bytes (Hex):");
    
    for (int i = 0; i < 32; i++) {
        print_hex_byte(sector_buffer[i]);
        print_char(' ');
        if ((i + 1) % 16 == 0) {
            print_char('\n');
        }
    }
}
// Функции cmd_nano и cmd_create теперь определены в commands/cmd_nano.cpp