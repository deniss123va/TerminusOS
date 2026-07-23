#include "panic.h"
#include "../lib/screen.h"

extern "C" {
    void panic(const char* message) {
        __asm__ volatile ("cli");

        uint16_t* vga = (uint16_t*) 0xB8000;
        uint8_t attr = 0x04; // Красный на черном

        // 1. Чистим экран
        for (int i = 0; i < 80 * 25; i++) vga[i] = (attr << 8) | ' ';

        // 2. Функция чистой печати без мусора в строках
        auto print_at = [&](int row, const char* text) {
            int len = 0;
            while (text[len]) len++;
            int col = (80 - len) / 2;
            for (int i = 0; i < len; i++) {
                vga[row * 80 + col + i] = (attr << 8) | text[i];
            }
        };

        // 3. Рисуем аккуратную рамку и текст (без лишних пробелов в кавычках!)
        print_at(9,  "########################################");
        print_at(10, "#             KERNEL PANIC             #");
        print_at(11, "########################################");
        
        print_at(14, "CRITICAL ERROR:");
        print_at(16, message); // Сообщение теперь будет точно по центру
        
        print_at(20, "System Halted. Please press Reset.");

        update_vga_cursor(0, 25); 
        while(1) __asm__ volatile ("hlt");
    }
}