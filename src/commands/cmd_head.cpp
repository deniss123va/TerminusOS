#include "cmd_head.h"
#include "../lib/screen.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"
#include "../lib/string.h"

extern uint8_t sector_buffer[512];
extern void ata_read_sector(uint32_t lba);

void cmd_head(const char* args) {
    if (!args || !*args) { println("Usage: head <file> [N]"); return; }

    // парсим: "filename" или "filename 10"
    char name[128] = {0};
    int  n_lines   = 10;
    int  i = 0;
    while (args[i] && args[i] != ' ' && i < 127) { name[i] = args[i]; i++; }
    name[i] = 0;
    if (args[i] == ' ') {
        i++;
        n_lines = 0;
        while (args[i] >= '0' && args[i] <= '9') n_lines = n_lines*10 + (args[i++]-'0');
        if (n_lines == 0) n_lines = 10;
    }

    FAT32_FindResult r = fat32_find_entry(name, 0x00);
    if (!r.found)              { println("head: file not found"); return; }
    if (r.entry.attributes & 0x10) { println("head: is a directory"); return; }

    uint32_t start = FAT32_GET_CLUSTER(&r.entry);
    uint32_t size  = r.entry.file_size;
    if (start < 2) { println("head: invalid cluster"); return; }

    ata_read_sector(fat32_cluster_to_lba(start));
    uint32_t limit = size < 512 ? size : 512;
    int printed = 0;
    for (uint32_t j = 0; j < limit && printed < n_lines; j++) {
        char c = (char)sector_buffer[j];
        print_char(c);
        if (c == '\n') printed++;
    }
    if (printed == 0) print_char('\n');
}