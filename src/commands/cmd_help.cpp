#include "cmd_help.h"
#include "../lib/screen.h"

void cmd_help(){
    println("Available commands:");
    println("  help     - Show this help        date     - Show date/time      ");
    println("  clear    - Clear screen          ls       - List files          ");
    println("  cat      - Show file contents    pwd      - Show current dir    ");
    println("  cd       - Change directory      mkdir    - Create directory    ");
    println("  rm       - Delete file/dir       mv       - Rename/move file    ");
    println("  cp       - Copy file             create   - Create empty file   ");
    println("  nano     - Text editor           theme    - Change color theme  ");
    println("  fatcheck - Check FAT table       fsd      - Format disk (WARN!) ");
    println("  info     - System info           exit     - Halt system         ");
    println("  reboot   - Reboot system         uptime   - Show uptime         ");
    println("  calc     - Calculator            wc       - Count lines/words   ");
    println("  head     - First N lines of file hexdump  - Hex view of file    ");
    println("  banner   - Big ASCII-art text    echo     - Print text          ");
}