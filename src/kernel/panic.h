#ifndef PANIC_H
#define PANIC_H

extern "C" {
    // Останавливает систему с сообщением об ошибке
    void panic(const char* message);
}

#endif // PANIC_H