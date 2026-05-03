#include "builtin.h"
#include "../lib/screen.h"
#include "../kernel/keyboard.h"
#include "../lib/cmd_settings.h"
#include "../drivers/disk.h"
#include "../drivers/fat32.h"
#include "../lib/string.h"
#include "../lib/utils.h"
#include "shell.h"
#include "../commands/cmd_info.h"
#include "../commands/cmd_nano.h"
#include "../commands/cmd_help.h"
#include "../commands/cmd_clear.h"
#include "../commands/cmd_wc.h"
#include "../commands/cmd_banner.h"
#include "../commands/cmd_uptime.h"
#include "../commands/cmd_hexdump.h"
#include "../commands/cmd_head.h"
#include "../commands/cmd_calc.h"
#include "../commands/cmd_panic.h"
#include "../drivers/rtc.h"

static uint8_t cp_buffer[16384];

int my_atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
        if (str[i] >= '0' && str[i] <= '9') res = res * 10 + str[i] - '0';
    return res;
}

void cmd_pwd() { println(current_path); }

void cmd_history() {
    if (history_count == 0) { println("History is empty."); return; }
    int start_idx = HISTORY_SIZE - history_count;
    for (int i = 0; i < history_count; i++) {
        // Номер
        int n = i + 1;
        char nbuf[8];
        int ni = 0;
        if (n >= 10) nbuf[ni++] = (char)('0' + n / 10);
        nbuf[ni++] = (char)('0' + n % 10);
        nbuf[ni++] = ' '; nbuf[ni++] = ' '; nbuf[ni] = 0;
        print(nbuf);
        println(history[start_idx + i]);
    }
}

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

    uint8_t attr_file = (theme_bg << 4) | (theme_file & 0x0F);
    uint8_t attr_dir  = (theme_bg << 4) | (theme_dir  & 0x0F);

    // Буфер для LFN (до 32 символов)
    char lfn_buf[33];
    int  lfn_len = 0;

    while ((c & FAT32_MASK) >= 2 && (c & FAT32_MASK) < (FAT32_EOC & FAT32_MASK)) {
        uint32_t base_lba = fat32_cluster_to_lba(c);
        for (uint8_t sec = 0; sec < FAT32_SECTORS_PER_CLUSTER; sec++) {
            ata_read_sector(base_lba + sec);
            for (int i = 0; i < FAT32_ENTRIES_PER_SECTOR; i++) {
                FAT32_DirEntry* e = (FAT32_DirEntry*)(sector_buffer + i * FAT32_ENTRY_SIZE);
                if (e->name[0] == 0x00) goto ls_end;
                if ((uint8_t)e->name[0] == 0xE5) { lfn_len = 0; continue; }
                if (e->attributes == 0x08) continue; // volume label

                // LFN запись
                if (e->attributes == 0x0F) {
                    // Структура LFN: bytes 1-10 (5 UCS-2), 14-23 (6), 28-31 (2) = 13 символов
                    // Берём только младший байт каждого UCS-2 символа
                    uint8_t* raw = (uint8_t*)e;
                    // Определяем порядковый номер (бит 6 = последняя запись)
                    uint8_t ord = raw[0] & 0x3F;
                    // Для простоты накапливаем LFN в обратном порядке и потом переворачиваем
                    // Но т.к. LFN-записи идут от последней к первой, просто читаем символы
                    char chunk[13]; int ci = 0;
                    // bytes 1,3,5,7,9 (5 символов)
                    for (int k = 1; k <= 9; k += 2) {
                        if (raw[k] != 0xFF && raw[k] != 0x00) chunk[ci++] = (char)raw[k];
                        else if (raw[k] == 0x00) chunk[ci++] = 0;
                    }
                    // bytes 14,16,18,20,22,24 (6 символов)
                    for (int k = 14; k <= 24; k += 2) {
                        if (raw[k] != 0xFF && raw[k] != 0x00) chunk[ci++] = (char)raw[k];
                        else if (raw[k] == 0x00) chunk[ci++] = 0;
                    }
                    // bytes 28,30 (2 символа)
                    for (int k = 28; k <= 30; k += 2) {
                        if (raw[k] != 0xFF && raw[k] != 0x00) chunk[ci++] = (char)raw[k];
                        else if (raw[k] == 0x00) chunk[ci++] = 0;
                    }
                    // Вставляем в начало lfn_buf (LFN-записи идут в обратном порядке)
                    // Сдвигаем существующее содержимое вправо
                    int chlen = 0;
                    while (chlen < 13 && chunk[chlen] != 0) chlen++;
                    if (lfn_len + chlen < 32) {
                        for (int k = lfn_len - 1; k >= 0; k--)
                            lfn_buf[k + chlen] = lfn_buf[k];
                        for (int k = 0; k < chlen; k++)
                            lfn_buf[k] = chunk[k];
                        lfn_len += chlen;
                    }
                    lfn_buf[lfn_len] = 0;
                    continue;
                }

                bool is_dir = (e->attributes & 0x10) != 0;
                uint8_t attr = is_dir ? attr_dir : attr_file;

                // Выводим имя: LFN если есть, иначе 8.3
                if (lfn_len > 0) {
                    for (int j = 0; j < lfn_len; j++)
                        print_char_colored(lfn_buf[j], attr);
                    lfn_len = 0;
                } else {
                    // 8.3 имя
                    for (int j = 0; j < 8; j++)
                        if (e->name[j] != ' ') print_char_colored(e->name[j], attr);
                    if (!is_dir) {
                        bool has_ext = false;
                        for (int j = 0; j < 3; j++) if (e->ext[j] != ' ') { has_ext = true; break; }
                        if (has_ext) {
                            print_char_colored('.', attr);
                            for (int j = 0; j < 3; j++)
                                if (e->ext[j] != ' ') print_char_colored(e->ext[j], attr);
                        }
                    }
                }

                // Тег <DIR> или размер
                if (is_dir) {
                    const char* tag = " <DIR>";
                    for (int j = 0; tag[j]; j++) print_char_colored(tag[j], attr);
                } else {
                    uint32_t sz = e->file_size;
                    char szbuf[12]; int si = 0;
                    if (sz == 0) { szbuf[si++] = '0'; }
                    else {
                        char tmp[12]; int ti = 0;
                        while (sz > 0) { tmp[ti++] = (char)('0' + sz % 10); sz /= 10; }
                        for (int j = ti - 1; j >= 0; j--) szbuf[si++] = tmp[j];
                    }
                    szbuf[si++] = 'B'; szbuf[si] = 0;
                    print_char_colored(' ', attr_file);
                    for (int j = 0; j < si; j++) print_char_colored(szbuf[j], attr_file);
                }
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
        println("Presets: matrix, ocean, amber, red");
        return;
    }

    if (strcmp(arg,"matrix")==0 || strcmp(arg,"ocean")==0 ||
        strcmp(arg,"amber")==0  || strcmp(arg,"red")==0 || strcmp(arg,"default")==0)  {
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

        uint8_t bg=0, fg=7, bar_bg=7, bar_fg=0, cursor=7,
                cursor_bg=7, cursor_char=0, dir=11, file=7;
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
                    if     (strcmp(key,"BG"         )==0) bg          = cv;
                    else if(strcmp(key,"FG"         )==0) fg          = cv;
                    else if(strcmp(key,"BAR_BG"     )==0) bar_bg      = cv;
                    else if(strcmp(key,"BAR_FG"     )==0) bar_fg      = cv;
                    else if(strcmp(key,"CURSOR"     )==0) cursor      = cv;
                    else if(strcmp(key,"CURSOR_BG"  )==0) cursor_bg   = cv;
                    else if(strcmp(key,"CURSOR_CHAR")==0) cursor_char = cv;
                    else if(strcmp(key,"DIR"        )==0) dir         = cv;
                    else if(strcmp(key,"FILE"       )==0) file        = cv;
                }
                line_start=&ptr[i+1];
            }
        }
        set_custom_theme(bg, fg, bar_bg, bar_fg, cursor, cursor_bg, cursor_char, dir, file);
    }

apply_theme:
    uint8_t attr = get_theme_color();
    uint16_t blank = (attr << 8) | ' ';
    for(int i=80;i<80*25;i++) video_memory[i]=blank;
    cmd_clear();
    shell_draw_status_bar();
    println("Custom theme loaded.");
    print("\r");
}