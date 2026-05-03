#ifndef CMD_REGISTRY_H
#define CMD_REGISTRY_H

#include <stdint.h>

// Тип аргумента команды
enum CmdArgType {
    ARG_NONE   = 0,   // аргументы не нужны:          clear, ls, pwd, date...
    ARG_TEXT   = 1,   // произвольный текст:           echo, theme, calc, banner...
    ARG_FILE   = 2,   // путь к файлу/директории:      cat, nano, rm, cd...
    ARG_OPT    = 3,   // аргумент опциональный:        ls (можно без), cp src dst
};

struct CmdEntry {
    const char* name;             // имя команды
    CmdArgType  arg_type;         // тип аргумента (для таба и проверки)
    void      (*fn_noarg)();      // вызов без аргумента
    void      (*fn_arg)(const char*); // вызов с аргументом
    const char* usage;            // строка Usage (показывается если аргумент нужен но не дан)
};

// Главная таблица команд
extern const CmdEntry CMD_TABLE[];
extern const int      CMD_TABLE_SIZE;

// Диспетчер — разбирает buffer и вызывает нужную команду
void cmd_dispatch(const char* buffer);

#endif