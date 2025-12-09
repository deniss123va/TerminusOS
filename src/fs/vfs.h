#ifndef VFS_H
#define VFS_H

// Старая структура VFS (оставляем для совместимости с командой 'ls')
struct File {
    const char* name;
    const char* content;
};

#define MAX_FILES 4

extern File fs[MAX_FILES];

#endif // VFS_H