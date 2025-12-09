#include "string.h"

// Строковые функции
int strcmp(const char* a, const char* b) {
    int i=0;
    while(a[i] && a[i]==b[i]) i++;
    return a[i]-b[i];
}

int strncmp(const char* a, const char* b, int n) {
    for(int i=0;i<n;i++){
        if(a[i]!=b[i]) return a[i]-b[i];
        if(a[i]==0) return 0;
    }
    return 0;
}

int strlen(const char* s) {
    int len = 0;
    while(s[len] != 0) len++;
    return len;
}

// Копирует строку
void strcpy(char* dest, const char* src) {
    int i = 0;
    while ((dest[i] = src[i]) != 0) i++;
}

// Добавляет src к концу dest
void strcat(char* dest, const char* src) {
    int i = strlen(dest);
    int j = 0;
    while(src[j]) dest[i++] = src[j++];
    dest[i] = 0;
}