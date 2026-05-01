#include "cmd_info.h"
#include "../../kernel/screen.h"

void cmd_info(){
    println("================================================================================");
    println("             _____ ____ ___  __  __ ___ _  _ _   _ ___   ___  ___");
    println("            |_   _| __|| _ \\|  \\/  |_ _| \\| | | / __| / _ \\/ __|");
    println("              | | | _| |   /| |\/| || || .` | |_\\__ \\| (_) \\__ \\");
    println("              |_| |___|_|_\\|_|  |_|___|_|\\_|____||___/ \\___/|___/");
    println("                                 v0.4.0");
    println("================================================================================");
    println("");
    println("  Author: Denis  |  Telegram: @den2010991  |  GitHub: github.com/deniss123va");
    println("");
    println("  [v0.4.0 UPDATES]");
    println("  FAT32: filesystem fully migrated from FAT16");
    println("  Themes: matrix / ocean / amber / default");
    println("  boot.cfg: theme loaded on startup automatically");
    println("  Tab show filenames");
    println("  New cmd: cp, reboot, shutdown, theme");
    println("  Status bar and all output respect active theme");
    println("");
    println("  Type 'help' for all commands");
    println("================================================================================");
}