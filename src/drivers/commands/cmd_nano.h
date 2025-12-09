#ifndef CMD_NANO_H
#define CMD_NANO_H

extern "C" {
    // Запускает текстовый редактор Nano-lite
    void cmd_nano();
    // Создает файл из содержимого буфера редактора
    void cmd_create(char* filename);
}

#endif // CMD_NANO_H