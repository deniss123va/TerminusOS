#include "cmd_uptime.h"
#include "../lib/screen.h"

// Счётчик итераций главного цикла.
// Главный цикл крутится ~1000 раз/сек (зависит от железа/QEMU),
// поэтому делим на TICKS_PER_SEC для приблизительного времени.
#define TICKS_PER_SEC 1000

static uint32_t uptime_ticks = 0;

void cmd_uptime_tick() {
    uptime_ticks++;
}

static void print_num2(uint32_t n) {
    if (n < 10) print_char('0');
    char buf[12]; int i = 0;
    if (n == 0) { print_char('0'); return; }
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    for (int j = i-1; j >= 0; j--) print_char(buf[j]);
}

void cmd_uptime() {
    uint32_t secs  = uptime_ticks / TICKS_PER_SEC;
    uint32_t mins  = secs / 60;   secs %= 60;
    uint32_t hours = mins / 60;   mins %= 60;
    uint32_t days  = hours / 24;  hours %= 24;

    print("up ");
    if (days)  { print_num2(days);  print("d "); }
    print_num2(hours); print(":");
    print_num2(mins);  print(":");
    print_num2(secs);
    print("  (ticks: ");
    // print raw ticks
    char buf[12]; int i = 0;
    uint32_t t = uptime_ticks;
    if (t == 0) { print_char('0'); }
    else { while (t) { buf[i++] = '0' + t%10; t /= 10; } for (int j=i-1;j>=0;j--) print_char(buf[j]); }
    println(")");
}