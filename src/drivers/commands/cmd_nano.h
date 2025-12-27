#ifndef CMD_NANO_H
#define CMD_NANO_H

extern "C" {
    // Запускает текстовый редактор (создание или редактирование)
    void cmd_nano(char* filename);
    
    // Создает пустой файл
    void cmd_create(char* filename);
}

#endif // CMD_NANO_H
