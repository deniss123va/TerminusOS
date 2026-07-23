#include "utils.h"
#include "string.h"

void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void io_wait(void) {
    outb(0x80, 0);
}

// ✅ НОВАЯ ФУНКЦИЯ: Убирает последний компонент из пути
void dirname(char* path) {
    int len = strlen(path);
    
    // Если путь это "/", оставляем как есть
    if (len <= 1) {
        return;
    }
    
    // Ищем последний '/'
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    // Если '/' не найден или это корень
    if (last_slash <= 0) {
        strcpy(path, "/");
    } else {
        // Обрезаем путь после последнего '/'
        path[last_slash] = 0;
    }
}
