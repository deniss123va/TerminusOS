#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

extern "C" {
    int strcmp(const char* a, const char* b);
    int strncmp(const char* a, const char* b, int n);
    int strlen(const char* s);
    void strcpy(char* dest, const char* src);
    void strcat(char* dest, const char* src);
    void memset(void* ptr, int value, size_t num);
    void* memcpy(void* dest, const void* src, size_t n);
}

#endif // STRING_H