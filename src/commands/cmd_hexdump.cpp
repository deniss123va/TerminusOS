#include "cmd_hexdump.h"
#include "../lib/screen.h"
#include "../drivers/fat32.h"
#include "../drivers/disk.h"
#include "../lib/string.h"

extern uint8_t sector_buffer[512];
extern bool ata_read_sector(uint32_t lba);

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

static void print_dec32(uint32_t v) {
    char buf[12]; int i = 0;
    if (v == 0) { print_char('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + v % 10); v /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// ─── буферы для полного дампа / сборки обратно ────────────────────────────────
// 1MB — тот же потолок, что у fat32_create_file (256 кластеров * 4096 байт):
// больше всё равно ни прочитать в файл, ни записать не выйдет, так что нет
// смысла резервировать больше.
#define HEXDUMP_MAX_BYTES (1024 * 1024)
static uint8_t hex_bin_buf[HEXDUMP_MAX_BYTES];
static char    hex_text_buf[HEXDUMP_MAX_BYTES];

// Читает файл `name` (из текущей директории) ПОЛНОСТЬЮ, по всей цепочке
// кластеров, в буфер out (вместимостью out_cap). Возвращает прочитанный
// размер, либо -1 при ошибке (сообщение уже напечатано).
static int32_t hexdump_read_whole(const char* name, uint8_t* out, uint32_t out_cap) {
    FAT32_FindResult r = fat32_find_entry(name, 0x00);
    if (!r.found)                  { println("hexdump: file not found"); return -1; }
    if (r.entry.attributes & 0x10) { println("hexdump: is a directory"); return -1; }

    uint32_t size = r.entry.file_size;
    if (size > out_cap) { println("hexdump: file too large (max 1MB)"); return -1; }

    uint32_t cluster     = FAT32_GET_CLUSTER(&r.entry);
    uint32_t bytes_read  = 0;
    while ((cluster & FAT32_MASK) >= 2 &&
           (cluster & FAT32_MASK) < (FAT32_EOC & FAT32_MASK) &&
           bytes_read < size) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for (int s = 0; s < FAT32_SECTORS_PER_CLUSTER && bytes_read < size; s++) {
            ata_read_sector(lba + s);
            for (int b = 0; b < FAT32_BYTES_PER_SECTOR && bytes_read < size; b++)
                out[bytes_read++] = sector_buffer[b];
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return (int32_t)bytes_read;
}

// Быстрый просмотр в терминале (старое поведение, без изменений) — первые
// max 256 байт файла, с ASCII-колонкой сбоку.
static void hexdump_view(const char* name) {
    FAT32_FindResult r = fat32_find_entry(name, 0x00);
    if (!r.found)                  { println("hexdump: file not found"); return; }
    if (r.entry.attributes & 0x10) { println("hexdump: is a directory"); return; }

    uint32_t start = FAT32_GET_CLUSTER(&r.entry);
    uint32_t size   = r.entry.file_size;
    if (start < 2) { println("hexdump: invalid cluster"); return; }

    ata_read_sector(fat32_cluster_to_lba(start));
    uint32_t limit = size < 256 ? size : 256;   // показываем max 256 байт (16 строк)

    for (uint32_t off = 0; off < limit; off += 16) {
        print_hex32(off); print("  ");

        for (int b = 0; b < 16; b++) {
            if (off + b < limit) print_hex8(sector_buffer[off + b]);
            else print("  ");
            print_char(' ');
            if (b == 7) print_char(' ');
        }
        print(" |");
        for (int b = 0; b < 16 && off + b < limit; b++) {
            uint8_t c = sector_buffer[off + b];
            print_char(c >= 32 && c < 127 ? (char)c : '.');
        }
        println("|");
    }
    if (size > 256) {
        print("  ... (");
        print_dec32(size - 256);
        println(" more bytes)");
    }
    println("  (use 'hexdump <file> -f <out.txt>' to dump the whole file for editing)");
}

// ─── hexdump <file> -f <out.txt> — полный дамп в редактируемый текстовый файл ──
// Формат строки: "OFFSET XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX\n"
// OFFSET — 6 hex-цифр, дальше 16 байт через пробел. На последней неполной
// строке недостающие байты — "--" (парсер при сборке их пропускает).
static void hexdump_export(const char* src_name, const char* out_name) {
    int32_t size32 = hexdump_read_whole(src_name, hex_bin_buf, HEXDUMP_MAX_BYTES);
    if (size32 < 0) return;
    uint32_t size = (uint32_t)size32;

    const char* h = "0123456789ABCDEF";
    uint32_t tp = 0;
    for (uint32_t off = 0; off < (size == 0 ? 1 : size); off += 16) {
        if (tp + 64 > HEXDUMP_MAX_BYTES) {
            println("hexdump: dump too large to fit in one output file");
            return;
        }
        for (int sh = 20; sh >= 0; sh -= 4) hex_text_buf[tp++] = h[(off >> sh) & 0xF];
        for (int b = 0; b < 16; b++) {
            hex_text_buf[tp++] = ' ';
            uint32_t idx = off + (uint32_t)b;
            if (idx < size) {
                uint8_t v = hex_bin_buf[idx];
                hex_text_buf[tp++] = h[v >> 4];
                hex_text_buf[tp++] = h[v & 0xF];
            } else {
                hex_text_buf[tp++] = '-';
                hex_text_buf[tp++] = '-';
            }
        }
        hex_text_buf[tp++] = '\n';
        if (size == 0) break;
    }

    FAT32_FindResult ex = fat32_find_entry(out_name, 0x00);
    if (ex.found) {
        if (ex.entry.attributes & 0x10) { println("hexdump: output name is a directory"); return; }
        fat32_delete_entry(out_name, 0x00);
    }
    bool ok = fat32_create_file(out_name, hex_text_buf, tp);
    if (!ok) { println("hexdump: failed to write dump file"); return; }
    print("hexdump: wrote "); print_dec32(size); print(" bytes (");
    print_dec32(tp); println(" bytes of hex text). Edit it, then reassemble with -c.");
}

// ─── hexdump <file> -c <in.txt> — собрать бинарник обратно из hex-дампа ────────
// Перезаписывает <file> (удаляет и создаёт заново), если он уже существует.
static void hexdump_assemble(const char* target_name, const char* in_name) {
    int32_t tsize32 = hexdump_read_whole(in_name, (uint8_t*)hex_text_buf, HEXDUMP_MAX_BYTES);
    if (tsize32 < 0) return;
    uint32_t tsize = (uint32_t)tsize32;

    uint32_t bp = 0; // позиция в hex_bin_buf (результат сборки)
    uint32_t i  = 0;
    while (i < tsize) {
        // Пропускаем оффсет (до первого пробела, максимум 6 символов)
        int skip = 0;
        while (i < tsize && hex_text_buf[i] != '\n' && hex_text_buf[i] != ' ' && skip < 8) { i++; skip++; }

        for (int b = 0; b < 16 && i < tsize && hex_text_buf[i] != '\n'; b++) {
            while (i < tsize && hex_text_buf[i] == ' ') i++;
            if (i >= tsize || hex_text_buf[i] == '\n') break;
            char c1 = hex_text_buf[i];
            char c2 = (i + 1 < tsize) ? hex_text_buf[i + 1] : 0;
            if (c1 == '-') { i += 2; continue; } // паддинг последней строки — пропускаем
            int v1 = hex_val(c1), v2 = hex_val(c2);
            if (v1 >= 0 && v2 >= 0 && bp < HEXDUMP_MAX_BYTES)
                hex_bin_buf[bp++] = (uint8_t)((v1 << 4) | v2);
            i += 2;
        }
        while (i < tsize && hex_text_buf[i] != '\n') i++; // долистать остаток строки, если что-то не так
        if (i < tsize) i++; // пропустить '\n'
    }

    if (bp == 0) { println("hexdump: nothing parsed from dump file — is the format right?"); return; }

    FAT32_FindResult ex = fat32_find_entry(target_name, 0x00);
    if (ex.found) {
        if (ex.entry.attributes & 0x10) { println("hexdump: target is a directory"); return; }
        fat32_delete_entry(target_name, 0x00);
    }
    bool ok = fat32_create_file(target_name, (const char*)hex_bin_buf, bp);
    if (!ok) { println("hexdump: failed to write reassembled file"); return; }
    print("hexdump: reassembled "); print_dec32(bp); print(" bytes into '");
    print(target_name); println("'.");
}

void cmd_hexdump(const char* args) {
    if (!args || !*args) {
        println("Usage: hexdump <file>");
        println("       hexdump <file> -f <out.txt>   dump whole file to hex text (to edit bytes)");
        println("       hexdump <file> -c <in.txt>     rebuild <file> from an edited hex text");
        return;
    }

    char a0[128] = {0}, a1[16] = {0}, a2[128] = {0};
    int len = (int)strlen(args), i = 0, p = 0;

    while (i < len && args[i] != ' ' && p < 127) a0[p++] = args[i++];
    a0[p] = 0;
    while (i < len && args[i] == ' ') i++;

    if (i >= len) { hexdump_view(a0); return; }

    p = 0;
    while (i < len && args[i] != ' ' && p < 15) a1[p++] = args[i++];
    a1[p] = 0;
    while (i < len && args[i] == ' ') i++;

    p = 0;
    while (i < len && p < 127) a2[p++] = args[i++];
    a2[p] = 0;

    if (strcmp(a1, "-f") == 0) {
        if (a2[0] == 0) { println("Usage: hexdump <file> -f <out.txt>"); return; }
        hexdump_export(a0, a2);
    } else if (strcmp(a1, "-c") == 0) {
        if (a2[0] == 0) { println("Usage: hexdump <file> -c <in.txt>"); return; }
        hexdump_assemble(a0, a2);
    } else {
        println("hexdump: unknown flag (expected -f or -c)");
    }
}