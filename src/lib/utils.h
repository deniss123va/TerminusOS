#ifndef UTILS_H
#define UTILS_H

#include "../lib/string.h"

extern "C" {
    // Укорачивает строку dest до последнего '/' (для cd ..)
    void dirname(char* dest);
    
    // Вспомогательная функция для извлечения аргумента (часть команды)
    char* get_arg(char* command, char* output_buffer);
}

#endif // UTILS_H