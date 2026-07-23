#include "scrollback.h"

#define SCROLLBACK_LINES 300    // ~300 строк истории

static uint16_t sb_buf[SCROLLBACK_LINES][80];
static int sb_head  = 0;   // индекс самой старой строки
static int sb_count = 0;   // сколько строк сохранено

void scrollback_push(const uint16_t* row) {
    int idx;
    if (sb_count < SCROLLBACK_LINES) {
        idx = sb_count++;
    } else {
        idx = sb_head;
        sb_head = (sb_head + 1) % SCROLLBACK_LINES;
    }
    for (int i = 0; i < 80; i++) sb_buf[idx][i] = row[i];
}

int scrollback_count() { return sb_count; }

void scrollback_reset() { sb_head = 0; sb_count = 0; }

// offset 0 = самая свежая (последняя уехавшая строка)
const uint16_t* scrollback_get(int offset) {
    if (offset < 0 || offset >= sb_count) return 0;
    int idx = (sb_head + sb_count - 1 - offset + SCROLLBACK_LINES) % SCROLLBACK_LINES;
    return sb_buf[idx];
}