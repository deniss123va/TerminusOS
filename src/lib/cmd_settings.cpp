#include "cmd_settings.h"
#include "../lib/screen.h"
#include "../lib/settings.h"
#include "../lib/string.h"

void cmd_settings(const char* args) {
    SystemSettings* settings = settings_get();
    
    if (args[0] == '\0') {
        println("=== System Settings v0.4.0 ===");
        print("Input help: ");
        println(settings->input_help ? "[ON ]" : "[OFF]");
        println("Usage: settings inputhelp [on|off]");
        return;
    }
    
    char option[16], value[8];
    int i = 0, j = 0;
    
    while (args[i] && args[i] != ' ') option[j++] = args[i++];
    option[j] = '\0';
    while (args[i] == ' ') i++;
    
    j = 0;
    while (args[i] && args[i] != ' ') value[j++] = args[i++];
    value[j] = '\0';
    
    // Приводим option и value к нижнему регистру
    for (int k = 0; option[k]; k++) if (option[k] >= 'A' && option[k] <= 'Z') option[k] += 32;
    for (int k = 0; value[k];  k++) if (value[k]  >= 'A' && value[k]  <= 'Z') value[k]  += 32;

    if (strcmp(option, "inputhelp") == 0) {
        if (strcmp(value, "on") == 0) {
            settings->input_help = true;
            settings_save();
            println("Input help: ON");
        } else if (strcmp(value, "off") == 0) {
            settings->input_help = false;
            settings_save();
            println("Input help: OFF");
        } else {
            print("Input help: ");
            println(settings->input_help ? "ON" : "OFF");
        }
    } else {
        println("Unknown option: inputhelp");
    }
}