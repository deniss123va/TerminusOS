#include "serial.h"

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

void serial_init() {
    outb(COM1 + 1, 0x00); // отключаем IRQ COM-порта
    outb(COM1 + 3, 0x80); // DLAB=1
    outb(COM1 + 0, 0x03); // делитель = 3 -> 38400 бод
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); // 8N1, DLAB=0
    outb(COM1 + 2, 0xC7); // FIFO on, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // RTS/DSR set
}

static int tx_empty() {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!tx_empty());
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* s) {
    while (*s) serial_putc(*s++);
}

static const char HEX[] = "0123456789ABCDEF";

void serial_hex8(uint8_t v) {
    serial_putc(HEX[(v >> 4) & 0xF]);
    serial_putc(HEX[v & 0xF]);
}

void serial_dec(int v) {
    if (v < 0) { serial_putc('-'); v = -v; }
    if (v == 0) { serial_putc('0'); return; }
    char buf[12];
    int i = 0;
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) serial_putc(buf[--i]);
}
