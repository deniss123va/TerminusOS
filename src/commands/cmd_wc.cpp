#include "cmd_wc.h"
#include "../lib/screen.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"
#include "../lib/string.h"
#include "../kernel/panic.h"

extern uint8_t sector_buffer[512];
extern void ata_read_sector(uint32_t lba);

static void print_num(int n) {
    char buf[12]; int i = 0;
    if (n == 0) { print_char('0'); return; }
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    for (int j = i-1; j >= 0; j--) print_char(buf[j]);
}

void cmd_wc(const char* name) {
    if (!name || !*name) { println("Usage: wc <file>"); return; }

    FAT32_FindResult r = fat32_find_entry(name, 0x00);
    if (!r.found)              { println("wc: file not found"); return; }
    if (r.entry.attributes & 0x10) { println("wc: is a directory"); return; }

    uint32_t start = FAT32_GET_CLUSTER(&r.entry);
    uint32_t size  = r.entry.file_size;
    if (start < 2)             { println("wc: invalid cluster"); return; }

    int lines = 0, words = 0, chars = 0;
    bool in_word = false;
    uint32_t lba = fat32_cluster_to_lba(start);
    ata_read_sector(lba);

    uint32_t limit = size < 512 ? size : 512;
    for (uint32_t i = 0; i < limit; i++) {
        char c = (char)sector_buffer[i];
        chars++;
        if (c == '\n') lines++;
        bool space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (!space && !in_word) { words++; in_word = true; }
        if (space) in_word = false;
    }

    print("  lines: "); print_num(lines);
    print("  words: "); print_num(words);
    print("  chars: "); print_num(chars);
    print("  ");       print(name);
    print_char('\n');
}