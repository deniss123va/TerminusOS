#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

extern "C" {
    void serial_init();
    void serial_putc(char c);
    void serial_write(const char* s);
    void serial_hex8(uint8_t v);
    void serial_dec(int v);
}

#endif // SERIAL_H
