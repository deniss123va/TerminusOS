#include "vfs.h"

File fs[MAX_FILES] = {
    {"readme.txt", "Welcome to MyOS!\n"},
    {"hello.txt", "Hello, Linux-like OS\n"},
    {"note.txt", "This is a minimal FS\n"},
    {"data.txt", "1234567890\n"}
};