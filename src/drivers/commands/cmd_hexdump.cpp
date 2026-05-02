#include "cmd_hexdump.h"
#include "../../kernel/screen.h"
#include "../../fs/fat32.h"
#include "../../drivers/disk.h"
#include "../../lib/string.h"

extern uint8_t sector_buffer[512];
extern void ata_read_sector(uint32_t lba);

static void print_hex8(uint8_t v) {
    const char* h = "0123456789ABCDEF";
    print_char(h[v >> 4]);
    print_char(h[v & 0xF]);
}

static void print_hex32(uint32_t v) {
    print_hex8((v >> 24) & 0xFF);
    print_hex8((v >> 16) & 0xFF);
    print_hex8((v >>  8) & 0xFF);
    print_hex8( v        & 0xFF);
}

void cmd_hexdump(const char* name) {
    if (!name || !*name) { println("Usage: hexdump <file>"); return; }

    FAT32_FindResult r = fat32_find_entry(name, 0x00);
    if (!r.found)              { println("hexdump: file not found"); return; }
    if (r.entry.attributes & 0x10) { println("hexdump: is a directory"); return; }

    uint32_t start = FAT32_GET_CLUSTER(&r.entry);
    uint32_t size  = r.entry.file_size;
    if (start < 2) { println("hexdump: invalid cluster"); return; }

    ata_read_sector(fat32_cluster_to_lba(start));
    uint32_t limit = size < 256 ? size : 256;   // показываем max 256 байт (16 строк)

    for (uint32_t off = 0; off < limit; off += 16) {
        // адрес
        print_hex32(off); print("  ");

        // hex часть
        for (int b = 0; b < 16; b++) {
            if (off + b < limit) print_hex8(sector_buffer[off + b]);
            else print("  ");
            print_char(' ');
            if (b == 7) print_char(' ');
        }
        print(" |");
        // ascii часть
        for (int b = 0; b < 16 && off + b < limit; b++) {
            uint8_t c = sector_buffer[off + b];
            print_char(c >= 32 && c < 127 ? (char)c : '.');
        }
        println("|");
    }
    if (size > 256) {
        print("  ... (");
        // print remaining size
        uint32_t rest = size - 256;
        char buf[12]; int i = 0;
        while (rest > 0) { buf[i++] = '0' + rest % 10; rest /= 10; }
        for (int j = i-1; j >= 0; j--) print_char(buf[j]);
        println(" more bytes)");
    }
}