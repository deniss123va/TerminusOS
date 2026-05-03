#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

struct SystemSettings {
    bool input_help;  // Помощь ввода (автоисправление команд)
};

void settings_init();
void settings_load();
void settings_save();
SystemSettings* settings_get();
void settings_toggle_input_help();

#endif
