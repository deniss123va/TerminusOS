#include "iso9660.h"
#include "atapi.h"
#include "../lib/screen.h"
#include "../lib/string.h"
#include <stdint.h>

// ===== Глобальное состояние =====

bool     iso_mounted        = false;
uint32_t iso_root_lba       = 0;
uint32_t iso_root_size      = 0;
char     iso_current_path[ISO_MAX_PATH] = "/";

// Буфер для одного ISO-сектора (2048 байт)
// Статический — экономим стек (в ядре стек ограничен)
static uint8_t iso_buf[ISO_SECTOR_SIZE];

// ===== Вспомогательные строковые утилиты =====

// Сравнение без учёта регистра
static int iso_strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

// Копирует имя ISO-записи в dst, убирает ";1" версию
// ISO 9660 Level 1: имена заглавными буквами, версия в конце ";1"
static void iso_copy_name(char* dst, const char* src, int len) {
    int di = 0;
    for (int i = 0; i < len && di < ISO_MAX_NAME - 1; i++) {
        if (src[i] == ';') break;   // убираем ";1"
        // Оставляем символ как есть (на CD могут быть уже строчные через Joliet/Rock Ridge)
        dst[di++] = src[i];
    }
    // Убираем точку в конце имён директорий у некоторых дисков
    if (di > 0 && dst[di - 1] == '.') di--;
    dst[di] = 0;
}

// Простой itoa для вывода чисел
static void iso_print_uint(uint32_t n) {
    if (n == 0) { print("0"); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    for (int k = i - 1; k >= 0; k--) print_char(buf[k]);
}

// ===== Монтирование =====

bool iso_mount() {
    // Читаем PVD (сектор 16)
    if (!atapi_read_sector(ISO_PVD_SECTOR, iso_buf)) {
        println("[ISO] Failed to read PVD sector");
        return false;
    }

    ISO_PVD* pvd = (ISO_PVD*)iso_buf;

    // Проверяем сигнатуру "CD001"
    if (pvd->type != 1 ||
        pvd->id[0] != 'C' || pvd->id[1] != 'D' ||
        pvd->id[2] != '0' || pvd->id[3] != '0' || pvd->id[4] != '1') {
        println("[ISO] No valid ISO 9660 PVD found");
        return false;
    }

    // Извлекаем корневую директорию из PVD
    ISO_DirEntry* root = (ISO_DirEntry*)pvd->root_dir_entry;
    iso_root_lba  = iso_le32(root->extent_lba);
    iso_root_size = iso_le32(root->data_length);

    iso_mounted = true;
    strcpy(iso_current_path, "/");

    print("[ISO] Mounted: ");
    // Выводим volume label (32 байта, с пробелами)
    char vlabel[33];
    for (int i = 0; i < 32; i++) vlabel[i] = pvd->volume_id[i];
    vlabel[32] = 0;
    // Убираем trailing spaces
    for (int i = 31; i >= 0 && vlabel[i] == ' '; i--) vlabel[i] = 0;
    println(vlabel);

    return true;
}

void iso_unmount() {
    iso_mounted = false;
    strcpy(iso_current_path, "/");
    iso_root_lba = 0;
    iso_root_size = 0;
}

// ===== Листинг директории =====

void iso_ls(uint32_t dir_lba, uint32_t dir_size) {
    uint32_t bytes_left = dir_size;
    uint32_t cur_lba    = dir_lba;

    while (bytes_left > 0) {
        if (!atapi_read_sector(cur_lba, iso_buf)) break;

        uint32_t offset = 0;
        uint32_t sec_bytes = (bytes_left < ISO_SECTOR_SIZE) ? bytes_left : ISO_SECTOR_SIZE;

        while (offset < sec_bytes) {
            ISO_DirEntry* e = (ISO_DirEntry*)(iso_buf + offset);
            if (e->length == 0) break;  // конец записей в этом секторе

            // Пропускаем '.' и '..'
            if (e->name_length == 1 && (e->name[0] == '\x00' || e->name[0] == '\x01')) {
                offset += e->length;
                continue;
            }

            char name[ISO_MAX_NAME];
            iso_copy_name(name, e->name, e->name_length);

            bool is_dir = (e->flags & ISO_FLAG_DIR) != 0;
            uint32_t fsize = iso_le32(e->data_length);

            if (is_dir) {
                // Директории выводим с '/'
                print("[DIR]  ");
                println(name);
            } else {
                print("[FILE] ");
                print(name);
                print("  (");
                iso_print_uint(fsize);
                println(" bytes)");
            }

            offset += e->length;
        }

        bytes_left -= sec_bytes;
        cur_lba++;
    }
}

// ===== Поиск по имени в директории =====

ISO_FindResult iso_find(uint32_t dir_lba, uint32_t dir_size, const char* name) {
    ISO_FindResult res;
    res.found  = false;
    res.is_dir = false;
    res.lba    = 0;
    res.size   = 0;

    uint32_t bytes_left = dir_size;
    uint32_t cur_lba    = dir_lba;

    while (bytes_left > 0) {
        if (!atapi_read_sector(cur_lba, iso_buf)) break;

        uint32_t offset = 0;
        uint32_t sec_bytes = (bytes_left < ISO_SECTOR_SIZE) ? bytes_left : ISO_SECTOR_SIZE;

        while (offset < sec_bytes) {
            ISO_DirEntry* e = (ISO_DirEntry*)(iso_buf + offset);
            if (e->length == 0) break;

            // '.' и '..'
            if (e->name_length == 1 && (e->name[0] == '\x00' || e->name[0] == '\x01')) {
                offset += e->length;
                continue;
            }

            char ename[ISO_MAX_NAME];
            iso_copy_name(ename, e->name, e->name_length);

            if (iso_strcasecmp(ename, name) == 0) {
                res.found  = true;
                res.is_dir = (e->flags & ISO_FLAG_DIR) != 0;
                res.lba    = iso_le32(e->extent_lba);
                res.size   = iso_le32(e->data_length);
                return res;
            }

            offset += e->length;
        }

        bytes_left -= sec_bytes;
        cur_lba++;
    }

    return res;
}

// ===== Поиск по пути от корня =====
// path — путь относительно корня CD, без ведущего '/'
// Пустая строка или "/" — корень

ISO_FindResult iso_find_path(const char* path) {
    ISO_FindResult res;
    res.found  = true;
    res.is_dir = true;
    res.lba    = iso_root_lba;
    res.size   = iso_root_size;

    // Пустой путь или "/" — сам корень
    if (!path || path[0] == 0 || (path[0] == '/' && path[1] == 0)) return res;

    // Копируем путь для разбора
    char tmp[ISO_MAX_PATH];
    int tlen = strlen(path);
    if (tlen >= ISO_MAX_PATH) tlen = ISO_MAX_PATH - 1;
    for (int i = 0; i < tlen; i++) tmp[i] = path[i];
    tmp[tlen] = 0;

    // Пропускаем ведущий '/'
    int start = 0;
    if (tmp[0] == '/') start = 1;

    uint32_t cur_lba  = iso_root_lba;
    uint32_t cur_size = iso_root_size;

    int i = start;
    while (i <= tlen) {
        // Ищем следующий компонент пути
        int j = i;
        while (tmp[j] && tmp[j] != '/') j++;
        tmp[j] = 0;

        if (j == i) { i = j + 1; continue; } // пустой сегмент

        char* seg = tmp + i;

        ISO_FindResult found = iso_find(cur_lba, cur_size, seg);
        if (!found.found) {
            res.found = false;
            return res;
        }
        cur_lba  = found.lba;
        cur_size = found.size;
        res = found;

        i = j + 1;
    }

    return res;
}

// ===== Разрешить путь директории =====

bool iso_resolve_dir(const char* cd_path, uint32_t* out_lba, uint32_t* out_size) {
    // cd_path — путь внутри CD (например "/" или "/DIR/SUBDIR")
    if (!cd_path || cd_path[0] == 0 || strcmp(cd_path, "/") == 0) {
        *out_lba  = iso_root_lba;
        *out_size = iso_root_size;
        return true;
    }
    ISO_FindResult r = iso_find_path(cd_path);
    if (!r.found || !r.is_dir) return false;
    *out_lba  = r.lba;
    *out_size = r.size;
    return true;
}

// ===== Чтение файла на экран =====

void iso_cat(uint32_t lba, uint32_t size) {
    uint32_t bytes_left = size;
    uint32_t cur_lba    = lba;

    while (bytes_left > 0) {
        if (!atapi_read_sector(cur_lba, iso_buf)) {
            println("[ISO] Read error");
            break;
        }

        uint32_t chunk = (bytes_left < ISO_SECTOR_SIZE) ? bytes_left : ISO_SECTOR_SIZE;
        for (uint32_t i = 0; i < chunk; i++) {
            char c = (char)iso_buf[i];
            if (c == '\r') continue;
            if (c == 0) break;
            print_char(c);
        }

        bytes_left -= chunk;
        cur_lba++;
    }
    print_char('\n');
}

// ===== Чтение файла в буфер =====

uint32_t iso_read_file(uint32_t lba, uint32_t size, uint8_t* buf, uint32_t buf_size) {
    uint32_t bytes_left = size;
    if (bytes_left > buf_size) bytes_left = buf_size;
    uint32_t cur_lba    = lba;
    uint32_t written    = 0;

    while (bytes_left > 0) {
        if (!atapi_read_sector(cur_lba, iso_buf)) break;

        uint32_t chunk = (bytes_left < ISO_SECTOR_SIZE) ? bytes_left : ISO_SECTOR_SIZE;
        for (uint32_t i = 0; i < chunk && written < buf_size; i++) {
            buf[written++] = iso_buf[i];
        }

        bytes_left -= chunk;
        cur_lba++;
    }
    return written;
}
