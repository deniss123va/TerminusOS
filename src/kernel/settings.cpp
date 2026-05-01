#include "settings.h"
#include "../kernel/screen.h"
#include "../fs/fat32.h"
#include "../drivers/disk.h"
#include "../lib/string.h"

#define SETTINGS_FILE "boot.cfg"

static SystemSettings g_settings = { false };

void settings_init() {
    g_settings.input_help = false;
}

void settings_load() {
    FAT32_FindResult res = fat32_find_entry(SETTINGS_FILE, 0x00);
    if (!res.found) {
        println("Settings: File not found, using defaults.");
        return;
    }
    uint32_t cluster = FAT32_GET_CLUSTER(&res.entry);
    uint32_t size    = res.entry.file_size;
    if (size == 0 || size > sizeof(SystemSettings) + 64) {
        println("Settings: Invalid size.");
        return;
    }
    uint32_t lba = fat32_cluster_to_lba(cluster);
    ata_read_sector(lba);
    // Ищем поле "INPUT_HELP=1"
    char* buf = (char*)sector_buffer;
    for (uint32_t i = 0; i + 11 < size; i++) {
        if (strncmp(buf + i, "INPUT_HELP=", 11) == 0) {
            g_settings.input_help = (buf[i + 11] == '1');
            break;
        }
    }
    println("Settings: Loaded.");
}

void settings_save() {
    char data[64] = {0};
    data[0] = 'I'; data[1] = 'N'; data[2] = 'P'; data[3] = 'U'; data[4] = 'T';
    data[5] = '_'; data[6] = 'H'; data[7] = 'E'; data[8] = 'L'; data[9] = 'P';
    data[10] = '='; data[11] = g_settings.input_help ? '1' : '0'; data[12] = '\n';
    uint32_t len = 13;

    FAT32_FindResult res = fat32_find_entry(SETTINGS_FILE, 0x00);
    if (res.found) fat32_delete_entry(SETTINGS_FILE, 0x00);

    if (!fat32_create_file(SETTINGS_FILE, data, len)) {
        println("Settings: Failed to save.");
    }
}

SystemSettings* settings_get() { return &g_settings; }

void settings_toggle_input_help() {
    g_settings.input_help = !g_settings.input_help;
    settings_save();
}