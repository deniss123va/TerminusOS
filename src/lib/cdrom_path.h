#ifndef CDROM_PATH_H
#define CDROM_PATH_H

// Общие хелперы для работы с виртуальным путём /cdrom — используются в
// builtin.cpp (cd/ls/mkdir/rm/mv/cp) и в cmd_nano.cpp (открытие файлов с CD).

// Проверяет, указывает ли путь p на /cdrom (сам "/cdrom" или "/cdrom/...").
bool path_is_cdrom_prefixed(const char* p);

// Строит полный путь ВНУТРИ ISO-дерева (без префикса "/cdrom") из введённого
// имени: либо относительно iso_current_path (если name не абсолютный), либо
// абсолютно, если name сам начинается с "/cdrom". out должен быть >= ISO_MAX_PATH байт.
void build_iso_path(const char* name, char* out);

#endif // CDROM_PATH_H
