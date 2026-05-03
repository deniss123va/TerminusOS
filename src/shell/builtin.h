#ifndef BUILTIN_H
#define BUILTIN_H

#include <stdint.h>
#include "../commands/cmd_info.h"
#include "../commands/cmd_nano.h"
#include "../commands/cmd_data.h"
#include "../commands/cmd_help.h"
#include "../commands/cmd_clear.h"
#include "../commands/cmd_wc.h"
#include "../commands/cmd_banner.h"
#include "../commands/cmd_uptime.h"
#include "../commands/cmd_hexdump.h"
#include "../commands/cmd_head.h"
#include "../commands/cmd_calc.h"
#include "../commands/cmd_panic.h"
#include "../commands/cmd_meminfo.h"


// FAT32
extern uint32_t current_dir_cluster; // shell

// Shell commands
void cmd_pwd();
void cmd_history();
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
void cmd_meminfo();

#endif
