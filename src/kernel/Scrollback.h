#ifndef SCROLLBACK_H
#define SCROLLBACK_H

#include <stdint.h>

// Сохранить строку (80 uint16_t VGA ячеек) в кольцевой буфер
void scrollback_push(const uint16_t* row);

// Количество сохранённых строк (max SCROLLBACK_LINES)
int  scrollback_count();
void scrollback_reset();

// Получить строку: offset 0 = самая свежая (только что уехавшая), 1 = предыдущая ...
const uint16_t* scrollback_get(int offset);

#endif