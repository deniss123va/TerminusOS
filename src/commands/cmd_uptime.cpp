#include "cmd_uptime.h"
#include "../lib/screen.h"
#include "../drivers/rtc.h"

// ─── Boot timestamp (секунды с начала суток) ──────────────────────────────────
// Сохраняем при старте; при переходе через полночь прибавляем 86400.
// Для хобби-ОС этого более чем достаточно.

static uint32_t boot_seconds = 0;
static bool     boot_saved   = false;

// Перевод RTC_Time → секунды с начала суток (0..86399)
static uint32_t rtc_to_secs(const RTC_Time& t) {
    return (uint32_t)t.hour   * 3600
         + (uint32_t)t.minute * 60
         + (uint32_t)t.second;
}

// Вызвать один раз из kmain после rtc_init()
void uptime_init() {
    RTC_Time t = rtc_read_time();
    boot_seconds = rtc_to_secs(t);
    boot_saved   = true;
}

// ─── Вспомогательный вывод числа с ведущим нулём ─────────────────────────────
static void print_d2(uint32_t n) {
    if (n < 10) print_char('0');
    if (n == 0) { print_char('0'); return; }
    char buf[4]; int i = 0;
    while (n) { buf[i++] = '0' + n % 10; n /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}

// ─── Команда ─────────────────────────────────────────────────────────────────
void cmd_uptime() {
    if (!boot_saved) { println("uptime: not initialized"); return; }

    RTC_Time now = rtc_read_time();
    uint32_t now_secs = rtc_to_secs(now);

    // Обработка перехода через полночь
    uint32_t elapsed = (now_secs >= boot_seconds)
                     ? now_secs - boot_seconds
                     : now_secs + 86400 - boot_seconds;

    uint32_t secs  = elapsed % 60;
    uint32_t mins  = (elapsed / 60) % 60;
    uint32_t hours = (elapsed / 3600) % 24;
    uint32_t days  = elapsed / 86400;

    print("up ");
    if (days) { print_d2(days); print("d "); }
    print_d2(hours); print(":");
    print_d2(mins);  print(":");
    print_d2(secs);

    // Текущее время для справки
    print("  (now ");
    print_d2(now.hour); print(":");
    print_d2(now.minute); print(":");
    print_d2(now.second);
    println(")");
}