#ifndef BUILTIN_H
#define BUILTIN_H

#include <stdint.h>
#include "../drivers/commands/cmd_info.h"
#include "../drivers/commands/cmd_nano.h"
#include "../drivers/commands/cmd_data.h"
#include "../drivers/commands/cmd_help.h"
#include "../drivers/commands/cmd_clear.h"

// FAT32
extern uint32_t current_dir_cluster; // shell

// Shell commands
void cmd_pwd();
void cmd_cd(char* name);
void cmd_mkdir(char* name);
void cmd_rm(char* name);
void cmd_mv(char* args);
void cmd_cp(char* args);
void cmd_ls_disk();
void cmd_disk_cat(char* name);
void cmd_read_disk();
void cmd_fat_check();
void cmd_create(char* name);
void fat_format_disk();
void cmd_shutdown();
void cmd_reboot();
void cmd_settings(const char* args);

#endif
