#include "keyboard.h"
#include "screen.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t last_scancode = 0;
static bool extended = false;

char get_key() {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return 0;
    
    uint8_t scancode = inb(0x60);
    
    // Обработка расширенных скан-кодов (E0)
    if (scancode == 0xE0) {
        extended = true;
        return 0;
    }
    
    // Игнорируем release-события (старший бит = 1)
    if (scancode & 0x80) {
        extended = false;
        return 0;
    }
    
    char result = 0;
    
    if (extended) {
        // Расширенные коды (стрелки)
        switch (scancode) {
            case 0x48: result = CHAR_ARROW_UP; break;
            case 0x50: result = CHAR_ARROW_DOWN; break;
            case 0x4B: result = CHAR_ARROW_LEFT; break;
            case 0x4D: result = CHAR_ARROW_RIGHT; break;
        }
        extended = false;
    } else {
        // Обычные клавиши
        static const char scancode_to_ascii[] = {
            0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
            '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
            0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
            0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
            '*', 0, ' '
        };
        
        // Функциональные клавиши F1-F12
        if (scancode == 0x3D) {  // F3
            result = CHAR_F3;
        } else if (scancode == 0x3E) {  // F4
            result = CHAR_F4;
        } else if (scancode < sizeof(scancode_to_ascii)) {
            result = scancode_to_ascii[scancode];
        }
    }
    
    last_scancode = scancode;
    return result;
}
