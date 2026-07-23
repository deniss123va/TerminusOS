#include "keyboard.h"
#include "../lib/screen.h"
#include "../drivers/serial.h"

// Порты контроллера клавиатуры
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Скан-коды
#define SCAN_LSHIFT_PRESS 0x2A
#define SCAN_LSHIFT_RELEASE 0xAA
#define SCAN_RSHIFT_PRESS 0x36
#define SCAN_RSHIFT_RELEASE 0xB6
#define SCAN_CAPSLOCK       0x3A
#define SCAN_LCTRL_PRESS    0x1D
#define SCAN_LCTRL_RELEASE  0x9D

// Глобальные флаги
bool shift_pressed = false;
bool caps_lock     = false;
static bool ctrl_pressed = false;
// E0-префикс расширенных клавиш (стрелки, Del, Home и т.д.)
// Нужен чтобы не спутать "фиктивный" shift (E0+0x2A / E0+0xAA) с настоящим
static bool e0_prefix = false;

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

uint8_t get_key() {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    if (status & 0x01) {
        serial_write("[KBD] st=");
        serial_hex8(status);

        // Бит 5 = 1 означает данные от мыши — их вычитывает IRQ12/mouse_handle_packet,
        // здесь просто пропускаем (НЕ читая порт данных, иначе уведём байт у мыши)
        if (status & 0x20) {
            serial_write(" mouse-skip\n");
            return 0;
        }
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        serial_write(" sc=");
        serial_hex8(scancode);
        serial_write("\n");

        // ── E0-префикс: расширенные клавиши (стрелки, Del, Home …) ──────────
        // Запоминаем, что следующий байт идёт с E0-префиксом.
        if (scancode == 0xE0) { e0_prefix = true; return 0; }

        // ── Ctrl ─────────────────────────────────────────────────────────────
        if (scancode == SCAN_LCTRL_PRESS)   { ctrl_pressed = true;  e0_prefix = false; return 0; }
        if (scancode == SCAN_LCTRL_RELEASE) { ctrl_pressed = false; e0_prefix = false; return 0; }

        // ── Shift ─────────────────────────────────────────────────────────────
        // E0+0x2A и E0+0xAA — «фиктивный» shift, который клавиатура посылает
        // вокруг расширенных клавиш (стрелки, PgUp …).  Его НЕЛЬЗЯ трактовать
        // как нажатие/отпускание настоящего Shift, иначе shift_pressed
        // сбросится в false до того как мы обработаем стрелку.
        if (scancode == SCAN_LSHIFT_RELEASE || scancode == SCAN_RSHIFT_RELEASE) {
            if (!e0_prefix) shift_pressed = false;   // настоящий Shift-release
            e0_prefix = false;
            return 0;
        }
        if (scancode == SCAN_LSHIFT_PRESS || scancode == SCAN_RSHIFT_PRESS) {
            if (!e0_prefix) shift_pressed = true;    // настоящий Shift-press
            e0_prefix = false;
            return 0;
        }

        // ── Caps Lock ─────────────────────────────────────────────────────────
        if (scancode == SCAN_CAPSLOCK) {
            caps_lock = !caps_lock;
            e0_prefix = false;
            return 0;
        }

        // ── Отпускание любой клавиши (бит 7) ─────────────────────────────────
        if (scancode & 0x80) {
            e0_prefix = false;
            return 0;
        }

        // ── Ctrl+буква ────────────────────────────────────────────────────────
        if (ctrl_pressed) {
            e0_prefix = false;
            if (scancode == 0x21) return CHAR_CTRL_F;
            if (scancode == 0x2E) return CHAR_CTRL_C;
            if (scancode == 0x2F) return CHAR_CTRL_V;
            if (scancode == 0x1E) return shift_pressed ? CHAR_CTRL_SHIFT_A : CHAR_CTRL_A;
            if (scancode == 0x12) return CHAR_CTRL_E;
            if (scancode == 0x2C) return shift_pressed ? CHAR_CTRL_SHIFT_Z : CHAR_CTRL_Z;
            if (scancode == 0x25) return CHAR_CTRL_K;
            if (scancode == 0x2D) return shift_pressed ? CHAR_CTRL_SHIFT_X : CHAR_CTRL_X;
            if (scancode == 0x0E) return CHAR_CTRL_BACKSPACE;
            if (scancode == 0x23) return CHAR_CTRL_H;
        }

        // ── ESC ───────────────────────────────────────────────────────────────
        if (scancode == 0x01) {
            e0_prefix = false;
            if (ctrl_pressed)  return CHAR_CTRL_ESC;
            if (shift_pressed) return CHAR_SHIFT_ESC;
            return 27;
        }

        // ── Del / Home / End ──────────────────────────────────────────────────
        if (scancode == 0x53) { e0_prefix = false; return ctrl_pressed ? CHAR_CTRL_DEL : CHAR_DEL; }
        if (scancode == 0x47) { e0_prefix = false; return CHAR_HOME; }
        if (scancode == 0x4F) { e0_prefix = false; return CHAR_END;  }

        // ── Стрелки (с Shift, Ctrl или Ctrl+Shift) ───────────────────────────────
        if (scancode == 0x48) { e0_prefix = false; return shift_pressed ? CHAR_SHIFT_UP    : CHAR_ARROW_UP;    }
        if (scancode == 0x50) { e0_prefix = false; return shift_pressed ? CHAR_SHIFT_DOWN  : CHAR_ARROW_DOWN;  }
        if (scancode == 0x4B) {
            e0_prefix = false;
            if (ctrl_pressed && shift_pressed) return CHAR_CTRL_SHIFT_LEFT;
            if (ctrl_pressed)                  return CHAR_CTRL_LEFT;
            return shift_pressed ? CHAR_SHIFT_LEFT : CHAR_ARROW_LEFT;
        }
        if (scancode == 0x4D) {
            e0_prefix = false;
            if (ctrl_pressed && shift_pressed) return CHAR_CTRL_SHIFT_RIGHT;
            if (ctrl_pressed)                  return CHAR_CTRL_RIGHT;
            return shift_pressed ? CHAR_SHIFT_RIGHT : CHAR_ARROW_RIGHT;
        }

        // ── PgUp / PgDn ───────────────────────────────────────────────────────
        if (scancode == 0x49) { e0_prefix = false; return CHAR_PGUP; }
        if (scancode == 0x51) { e0_prefix = false; return CHAR_PGDN; }

        // ── Tab / Shift+Tab ───────────────────────────────────────────────────
        if (scancode == 0x0F) { e0_prefix = false; return shift_pressed ? (char)CHAR_SHIFT_TAB : (char)CHAR_TAB; }

        // ── Обычные символы ───────────────────────────────────────────────────
        e0_prefix = false;
        if (scancode < sizeof(scan_code_table)) {
            char c = 0;
            if (shift_pressed) {
                c = scan_code_table_shift[scancode];
            } else {
                c = scan_code_table[scancode];
                if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
            }
            return c;
        }
    }
    return 0;
}