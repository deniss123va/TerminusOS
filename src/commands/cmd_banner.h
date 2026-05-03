#ifndef CMD_BANNER_H
#define CMD_BANNER_H

// Вывести текст ASCII-артом (5×7 пиксельный шрифт).
// Поддерживаются: A-Z, 0-9, ! ? . -  и пробел.
// Максимальная длина — 12 символов (80 колонок).
void cmd_banner(const char* text);

#endif