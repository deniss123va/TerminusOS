#include "cmd_help.h"
#include "../../kernel/screen.h"

void cmd_help(){
    println("Available commands:");
    println("  help     - Show help             date     - Show date and time ");
    println("  clear    - Clear screen          ls       - List files         ");
    println("  cat      - Show file contents    pwd      - Show current dir   ");
    println("  cd       - Change directory      mkdir    - Create directory   ");
    println("  rm       - Delete file/dir       mv       - Rename/move file   ");
    println("  cp       - Copy file             create   - Create empty file  ");
    println("  nano     - Text editor           theme    - Change color theme ");
    println("  fatcheck - Check FAT table       fsd      - Format disk (WARN!)");
    println("  info     - System information    exit     - Halt system        ");
}