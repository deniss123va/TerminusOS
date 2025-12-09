#ifndef STRING_H
#define STRING_H

#include <stdint.h>

extern "C" {
    int strcmp(const char* a, const char* b);
    int strncmp(const char* a, const char* b, int n);
    int strlen(const char* s);
    void strcpy(char* dest, const char* src);
    void strcat(char* dest, const char* src);
}

#endif // STRING_H