#include "builtin.h"
#include "../kernel/screen.h"
#include "../kernel/keyboard.h"
#include "../kernel/cmd_settings.h"
#include "../drivers/disk.h"
#include "../fs/fat32.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "shell.h"
#include "../drivers/commands/cmd_info.h"
#include "../drivers/commands/cmd_nano.h"
#include "../drivers/commands/cmd_help.h"
#include "../drivers/commands/cmd_clear.h"
#include "../drivers/rtc.h"

static uint8_t cp_buffer[16384];

int my_atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
        if (str[i] >= '0' && str[i] <= '9') res = res * 10 + str[i] - '0';
    return res;
}

void cmd_pwd() { println(current_path); }

void cmd_fat_check() {
    println("=== FAT32 Table Check ===");
    for (uint32_t c = 0; c < 20; c++) fat32_debug_cluster_state(c);
    println("=== End Check ===");
}

void cmd_cd(char* name) {
    if (strcmp(name, "/") == 0) {
        current_dir_cluster = FAT32_ROOT_CLUSTER;
        strcpy(current_path, "/");
        println("Changed to /");
        return;
    }
    if (strcmp(name, ".") == 0) { println("Directory remains the same."); return; }

    FAT32_FindResult result = fat32_find_entry(name, 0x10);
    if (!result.found)                    { println("Error: Directory not found."); return; }
    if (!(result.entry.attributes & 0x10)){ println("Error: This is a file, not a directory!"); return; }

    uint32_t target_cluster = FAT32_GET_CLUSTER(&result.entry);

    if (strcmp(name, "..") == 0) {
        if (current_dir_cluster == FAT32_ROOT_CLUSTER) { println("Already in root directory."); return; }
        dirname(current_path);
        current_dir_cluster = (target_cluster < 2) ? FAT32_ROOT_CLUSTER : target_cluster;
        println("Changed to parent directory.");
    } else {
        if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
        strcat(current_path, name);
        current_dir_cluster = (target_cluster < 2) ? FAT32_ROOT_CLUSTER : target_cluster;
        println("Changed directory.");
    }
}

void cmd_mkdir(char* name) {
    if (strlen(name) == 0) { println("Usage: mkdir <n>"); return; }
    if (fat32_find_entry(name, 0x00).found) { println("Error: Item already exists."); return; }
    fat32_create_dir(name);
}

void cmd_rm(char* name) {
    if (strcmp(name,".") == 0 || strcmp(name,"..") == 0 || strcmp(name,"/") == 0) {
        println("Error: Cannot remove special directories."); return;
    }
    fat32_delete_entry(name, 0x00);
}

void cmd_mv(char* args) {
    char old_name[128]={0}, new_name[128]={0};
    int len = strlen(args), sp = 0;
    while(sp < len && args[sp] != ' ') sp++;
    if (sp == len || sp == 0) { println("Usage: mv <old> <new>"); return; }
    for(int i=0;i<sp;i++) old_name[i]=args[i]; old_name[sp]=0;
    int ns=sp+1; for(int i=ns;i<len;i++) new_name[i-ns]=args[i];
    if (strlen(new_name)==0) { println("Usage: mv <old> <new>"); return; }
    fat32_rename_entry(old_name, new_name);
}

void cmd_cp(char* args) {
    char src[128]={0}, dst[128]={0};
    int len=strlen(args), sp=0;
    while(sp<len && args[sp]!=' ') sp++;
    if (sp==len||sp==0) { println("Usage: cp <source> <dest>"); return; }
    for(int i=0;i<sp;i++) src[i]=args[i]; src[sp]=0;
    int ns=sp+1; for(int i=ns;i<len;i++) dst[i-ns]=args[i];
    if (strlen(dst)==0) { println("Usage: cp <source> <dest>"); return; }

    FAT32_FindResult res = fat32_find_entry(src, 0x00);
    if (!res.found) { println("Error: Source file not found."); return; }
    if (res.entry.attributes & 0x10) { println("Error: Cannot copy directory."); return; }

    uint32_t size = res.entry.file_size;
    if (size > sizeof(cp_buffer)) { println("Error: File too large for buffer. Max: 16KB"); return; }

    uint32_t cluster = FAT32_GET_CLUSTER(&res.entry);
    uint32_t bytes_read = 0;

    while ((cluster & FAT32_MASK) >= 2 &&
           (cluster & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) &&
           bytes_read < size) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for (int s = 0; s < FAT32_SECTORS_PER_CLUSTER && bytes_read < size; s++) {
            ata_read_sector(lba + s);
            for (int b = 0; b < FAT32_BYTES_PER_SECTOR && bytes_read < size; b++)
                cp_buffer[bytes_read++] = sector_buffer[b];
        }
        cluster = fat32_get_next_cluster(cluster);
    }

    if (fat32_find_entry(dst, 0x00).found) { println("Error: Destination already exists."); return; }
    println("Copying...");
    bool ok = fat32_create_file(dst, (char*)cp_buffer, size);
    if (ok) println("File copied successfully."); else println("Error: Failed to create destination file.");
}

void cmd_ls_disk() {
    println("--- Disk Directory (FAT32) ---");
    uint32_t c = current_dir_cluster;

    while ((c & FAT32_MASK) >= 2 && (c & FAT32_MASK) < (FAT32_EOC & FAT32_MASK)) {
        uint32_t base_lba = fat32_cluster_to_lba(c);
        for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER; sec++) {
            ata_read_sector(base_lba + sec);
            for (int i = 0; i < FAT32_ENTRIES_PER_SECTOR; i++) {
                FAT32_DirEntry* e = (FAT32_DirEntry*)(sector_buffer + i * FAT32_ENTRY_SIZE);
                if (e->name[0] == 0x00) goto ls_end;
                if ((uint8_t)e->name[0] == 0xE5 || e->attributes == 0x0F || e->attributes == 0x08) continue;
                for (int j=0;j<8;j++) if(e->name[j]!=' ') print_char(e->name[j]);
                if (!(e->attributes & 0x10)) { print_char('.'); for(int j=0;j<3;j++) if(e->ext[j]!=' ') print_char(e->ext[j]); }
                if (e->attributes & 0x10) print(" <DIR>");
                print_char('\n');
            }
        }
        c = fat32_get_next_cluster(c);
    }
ls_end:
    println("--- End of Directory ---");
}

void cmd_disk_cat(char* name) {
    FAT32_FindResult result = fat32_find_entry(name, 0x00);
    if (!result.found) { println("Error: File not found."); return; }
    if (result.entry.attributes & 0x10) { println("Error: This is a directory, not a file."); return; }

    uint32_t start_cluster = FAT32_GET_CLUSTER(&result.entry);
    uint32_t filesize = result.entry.file_size;
    if (start_cluster < 2) { println("Error: Invalid cluster."); return; }

    uint32_t data_lba = fat32_cluster_to_lba(start_cluster);
    ata_read_sector(data_lba);
    println("--- Content ---");
    for (uint32_t i = 0; i < filesize && i < FAT32_BYTES_PER_SECTOR; i++) print_char(sector_buffer[i]);
    print_char('\n');
    println("--- End ---");
}

void cmd_read_disk() {
    println("Reading Sector 0 (Boot Sector)...");
    ata_read_sector(0);
    println("First 32 bytes (Hex):");
    for (int i = 0; i < 32; i++) {
        print_hex_byte(sector_buffer[i]); print_char(' ');
        if ((i+1)%16==0) print_char('\n');
    }
}

void cmd_create(char* name) {
    if(strlen(name)==0) { println("Usage: create <filename>"); return; }
    if(fat32_find_entry(name, 0x00).found) { println("Error: File already exists."); return; }
    char empty[1]={0};
    bool ok = fat32_create_file(name, empty, 0);
    if(ok) { print("File '"); print(name); println("' created successfully."); }
    else println("Error: Failed to create file.");
}

void fat_format_disk() {
    println("FORMAT: Starting FAT32 format");
    uint8_t zero[FAT32_BYTES_PER_SECTOR] = {0};

    // 1. Очистка зарезервированных секторов
    for (uint32_t s = 0; s < FAT32_RESERVED_SECTORS; s++) ata_write_sector(s, zero);
    println("FORMAT: Reserved sectors cleared");

    // 2. Очистка FAT таблиц
    for (uint32_t fat = 0; fat < FAT32_NUMBER_OF_FATS; fat++) {
        uint32_t fat_base = FAT32_FAT_START + fat * FAT32_SECTORS_PER_FAT;
        for (uint32_t s = 0; s < FAT32_SECTORS_PER_FAT; s++) ata_write_sector(fat_base + s, zero);
    }
    println("FORMAT: FAT tables cleared");

    // 3. Служебные записи FAT32
    ata_read_sector(FAT32_FAT_START);
    uint32_t* fat32_tbl = (uint32_t*)sector_buffer;
    fat32_tbl[0] = 0x0FFFFFF8;
    fat32_tbl[1] = FAT32_EOC;
    fat32_tbl[2] = FAT32_EOC;  // Root cluster
    ata_write_sector(FAT32_FAT_START, sector_buffer);
    if (FAT32_NUMBER_OF_FATS > 1)
        ata_write_sector(FAT32_FAT_START + FAT32_SECTORS_PER_FAT, sector_buffer);
    println("FORMAT: FAT reserved clusters written");

    // 4. Очистка кластера корневой директории
    uint32_t root_lba = fat32_cluster_to_lba(FAT32_ROOT_CLUSTER);
    for (uint32_t s = 0; s < FAT32_SECTORS_PER_CLUSTER; s++) ata_write_sector(root_lba + s, zero);
    println("FORMAT: Root directory cleared");

    current_dir_cluster = FAT32_ROOT_CLUSTER;
    println("FORMAT: FAT32 format completed successfully");
}

void cmd_reboot() {
    println("Rebooting system...");
    uint8_t temp;
    do { temp = inb(0x64); if (temp & 1) inb(0x60); } while (temp & 2);
    outb(0x64, 0xFE);
    println("Error: Reboot failed.");
    while(1) asm volatile("hlt");
}

void cmd_shutdown() {
    println("Shutting down...");
    outw(0xB004, 0x2000);
    outw(0x604, 0x2000);
    println("It is now safe to turn off your computer.");
    while(1) { asm volatile("cli"); asm volatile("hlt"); }
}

void cmd_theme(char* arg) {
    if (strlen(arg) == 0) {
        println("Usage: theme <filename.thm> OR <preset>");
        println("Presets: matrix, ocean, amber");
        return;
    }

    if (strcmp(arg,"matrix")==0 || strcmp(arg,"ocean")==0 ||
        strcmp(arg,"amber")==0  || strcmp(arg,"default")==0) {
        set_theme(arg); goto apply_theme;
    }

    {
        FAT32_FindResult res = fat32_find_entry(arg, 0x00);
        if (!res.found) { println("Error: Theme file or preset not found."); return; }
        uint32_t size = res.entry.file_size;
        if (size > FAT32_BYTES_PER_SECTOR) { println("Error: Theme file too large."); return; }

        static uint8_t theme_buf[FAT32_BYTES_PER_SECTOR];
        uint32_t lba = fat32_cluster_to_lba(FAT32_GET_CLUSTER(&res.entry));
        ata_read_sector(lba);
        for(int i=0;i<FAT32_BYTES_PER_SECTOR;i++) theme_buf[i]=sector_buffer[i];
        theme_buf[size]=0;

        uint8_t bg=0, fg=7, bar_bg=7, bar_fg=0;
        char* ptr=(char*)theme_buf, *line_start=ptr;

        for(int i=0;i<=(int)size;i++) {
            if(ptr[i]=='\n'||ptr[i]==0) {
                ptr[i]=0;
                char* eq=0;
                for(char* p=line_start;*p;p++) if(*p=='=') eq=p;
                if(eq) {
                    *eq=0; char* key=line_start, *val=eq+1;
                    int vlen=strlen(val); if(vlen>0&&val[vlen-1]=='\r') val[vlen-1]=0;
                    int cv=my_atoi(val);
                    if(strcmp(key,"BG")==0) bg=cv;
                    else if(strcmp(key,"FG")==0) fg=cv;
                    else if(strcmp(key,"BAR_BG")==0) bar_bg=cv;
                    else if(strcmp(key,"BAR_FG")==0) bar_fg=cv;
                }
                line_start=&ptr[i+1];
            }
        }
        set_custom_theme(bg, fg, bar_bg, bar_fg);
        println("Custom theme loaded.");
    }

apply_theme:
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';
    for(int i=80;i<80*25;i++) video_memory[i]=blank;
    cmd_clear();
    shell_draw_status_bar();
    print("\r");
}
