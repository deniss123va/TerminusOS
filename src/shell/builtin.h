#ifndef BUILTIN_H
#define BUILTIN_H

extern "C" {

    // Disk/FAT16 commands
    void cmd_read_disk();
    void cmd_disk_cat(char* name); 
    void cmd_ls_disk();
    void fat_format_disk();
    
    // Directory commands
    void cmd_pwd();
    void cmd_cd(char* name);
    void cmd_mkdir(char* name);
    void cmd_rm(char* name);
    void cmd_mv(char* args);

    // Editor & File creation (implemented in commands/cmd_nano.cpp)
    void cmd_nano();
    void cmd_create(char* filename);
    void cmd_info();

    void cmd_fat_check();
}

#endif // BUILTIN_H