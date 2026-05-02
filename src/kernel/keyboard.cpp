#include "keyboard.h"
#include "../kernel/screen.h"

// Порты контроллера клавиатуры
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Скан-коды
#define SCAN_LSHIFT_PRESS 0x2A
#define SCAN_LSHIFT_RELEASE 0xAA
#define SCAN_RSHIFT_PRESS 0x36
#define SCAN_RSHIFT_RELEASE 0xB6
#define SCAN_CAPSLOCK 0x3A

// Глобальные флаги
bool shift_pressed = false;
bool caps_lock = false;

// Таблица соответствия скан-кодов ASCII символам (без Shift)
static char scan_code_table[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Таблица для Shift (символы верхнего регистра и спецсимволы)
static char scan_code_table_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

// Функция чтения с порта
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

char get_key() {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    
    // Если буфер полон (бит 0 установлен)
    if (status & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        
        // Обработка отпускания Shift
        if (scancode == SCAN_LSHIFT_RELEASE || scancode == SCAN_RSHIFT_RELEASE) {
            shift_pressed = false;
            return 0;
        }
        
        // Обработка нажатия Shift
        if (scancode == SCAN_LSHIFT_PRESS || scancode == SCAN_RSHIFT_PRESS) {
            shift_pressed = true;
            return 0;
        }

        // Обработка Caps Lock (только нажатие)
        if (scancode == SCAN_CAPSLOCK) {
            caps_lock = !caps_lock;
            return 0;
        }

        // Если это отпускание клавиши (бит 7 установлен), игнорируем (кроме Shift выше)
        if (scancode & 0x80) {
            return 0;
        }
        
        // Обработка стрелок (расширенные коды, упрощенно для примера)
        // В реальном драйвере нужно обрабатывать 0xE0 префикс корректно
        if (scancode == 0x48) return CHAR_ARROW_UP;
        if (scancode == 0x50) return CHAR_ARROW_DOWN;
        if (scancode == 0x4B) return CHAR_ARROW_LEFT;
        if (scancode == 0x4D) return shift_pressed ? (char)CHAR_SHIFT_RIGHT : (char)CHAR_ARROW_RIGHT;
        if (scancode == 0x49) return CHAR_PGUP;
        if (scancode == 0x51) return CHAR_PGDN;

        // Tab / Shift+Tab
        if (scancode == 0x0F) return shift_pressed ? (char)CHAR_SHIFT_TAB : (char)CHAR_TAB;

        // Преобразование в ASCII
        if (scancode < sizeof(scan_code_table)) {
            char c = 0;
            
            // Логика выбора таблицы
            if (shift_pressed) {
                // Если нажат Shift, берем из shift-таблицы
                // Учитываем Caps Lock для букв (инверсия)
                // Но проще просто взять из Shift таблицы, она уже содержит большие буквы
                // Если нужен правильный CapsLock + Shift (инверсия), логика сложнее
                
                // Простая логика: Shift имеет приоритет
                c = scan_code_table_shift[scancode];
                
                // Если CapsLock включен и это буква, можно инвертировать обратно в малую (опционально)
                // Но обычно Shift+Letter при CapsLock дает малую букву.
                // Пока оставим просто Shift таблицу.
            } else {
                c = scan_code_table[scancode];
                
                // Если CapsLock включен и это буква (a-z)
                if (caps_lock && c >= 'a' && c <= 'z') {
                    c -= 32; // Преобразуем в заглавную
                }
            }
            return c;
        }
    }
    return 0;
}