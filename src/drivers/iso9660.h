#ifndef ISO9660_H
#define ISO9660_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ===== ISO 9660 CD-ROM Filesystem =====
//
// Виртуальная файловая система /cdrom.
// Файлы читаются напрямую с CD; запись не поддерживается.

#define ISO_SECTOR_SIZE     2048
#define ISO_PVD_SECTOR      16      // Primary Volume Descriptor всегда в секторе 16
#define ISO_MAX_PATH        256
#define ISO_MAX_NAME        128

// Флаги записи директории
#define ISO_FLAG_DIR        0x02    // Бит 1: запись является директорией

// ===== Структуры ISO 9660 =====

// "Both-endian" форматы ISO 9660 хранят числа дважды: LE + BE
// Мы берём только LE (первые байты)

struct ISO_DirEntry {
    uint8_t  length;            // Длина всей записи
    uint8_t  ext_attr_length;
    uint8_t  extent_lba[8];     // LBA данных (LE 4 байта + BE 4 байта)
    uint8_t  data_length[8];    // Размер данных (LE 4 + BE 4)
    uint8_t  date[7];           // Дата/время записи
    uint8_t  flags;             // Флаги (DIR и т.д.)
    uint8_t  file_unit_size;
    uint8_t  interleave_gap;
    uint8_t  volume_seq[4];     // LE+BE
    uint8_t  name_length;
    char     name[1];           // Имя переменной длины (затем padding)
} __attribute__((packed));

struct ISO_PVD {
    uint8_t  type;              // 1 = Primary Volume Descriptor
    char     id[5];             // "CD001"
    uint8_t  version;
    uint8_t  unused1;
    char     system_id[32];
    char     volume_id[32];
    uint8_t  unused2[8];
    uint8_t  volume_space[8];   // LE+BE: число секторов
    uint8_t  unused3[32];
    uint8_t  volume_set_size[4];
    uint8_t  volume_seq[4];
    uint8_t  logical_block_size[4]; // LE+BE: обычно 2048
    uint8_t  path_table_size[8];
    uint8_t  path_table_lba_le[4];
    uint8_t  path_table_lba_le2[4];
    uint8_t  path_table_lba_be[4];
    uint8_t  path_table_lba_be2[4];
    uint8_t  root_dir_entry[34];    // Корневая запись директории
    char     volume_set_id[128];
    char     publisher_id[128];
    char     preparer_id[128];
    char     application_id[128];
    char     copyright_file[37];
    char     abstract_file[37];
    char     bibliographic_file[37];
    uint8_t  creation_date[17];
    uint8_t  modification_date[17];
    uint8_t  expiration_date[17];
    uint8_t  effective_date[17];
    uint8_t  file_structure_version;
    uint8_t  unused4;
    uint8_t  application_data[512];
    uint8_t  unused5[653];
} __attribute__((packed));

// ===== Результат поиска ISO-файла/папки =====

struct ISO_FindResult {
    bool     found;
    bool     is_dir;
    uint32_t lba;           // LBA данных
    uint32_t size;          // Размер в байтах (0 для директорий)
};

// ===== Состояние ISO-драйвера =====

// true если CD смонтирован
extern bool iso_mounted;

// Текущий путь внутри /cdrom (без "/cdrom" префикса; "/" = корень CD)
extern char iso_current_path[ISO_MAX_PATH];

// LBA корневой директории CD
extern uint32_t iso_root_lba;
extern uint32_t iso_root_size;

extern "C" {

// Монтирует ISO 9660 с диска. Возвращает true при успехе.
bool iso_mount();

// Размонтирует CD
void iso_unmount();

// Список файлов текущей ISO-директории (аналог ls)
void iso_ls(uint32_t dir_lba, uint32_t dir_size);

// Поиск записи по имени в директории
// dir_lba, dir_size — директория для поиска
// name — искомое имя (без версии ";1")
ISO_FindResult iso_find(uint32_t dir_lba, uint32_t dir_size, const char* name);

// Найти запись по абсолютному пути от корня CD (например "DIR/FILE.TXT")
ISO_FindResult iso_find_path(const char* path);

// Вывести содержимое файла на экран (аналог cat)
void iso_cat(uint32_t lba, uint32_t size);

// Прочитать содержимое файла в буфер (возвращает прочитанный размер)
uint32_t iso_read_file(uint32_t lba, uint32_t size, uint8_t* buf, uint32_t buf_size);

// Вспомогательная: получить LBA директории по текущему пути внутри CD
bool iso_resolve_dir(const char* cd_path, uint32_t* out_lba, uint32_t* out_size);

} // extern "C"

// Вспомогательная: прочитать LE uint32 из 4 байт ISO-структуры
static inline uint32_t iso_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Вспомогательная: LE uint16
static inline uint16_t iso_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

#endif // ISO9660_H
