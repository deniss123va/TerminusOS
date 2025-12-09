#include "keyboard.h"
#include "../drivers/disk.h" // Для inb

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

#define SC_ARROW_UP 0x48
#define SC_ARROW_DOWN 0x50
#define SC_ARROW_LEFT 0x4B
#define SC_ARROW_RIGHT 0x4D

bool shift_pressed = false; 

char scancode_to_ascii[128] = {0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

char scancode_to_shifted_ascii[128] = {0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','\"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


extern "C" char get_key(){
    unsigned char status = 0;
    __asm__ __volatile__("inb %1, %0":"=a"(status):"Nd"(KBD_STATUS));
    
    if(status & 1){
        unsigned char scancode;
        __asm__ __volatile__("inb %1, %0":"=a"(scancode):"Nd"(KBD_DATA));
        
        if (scancode == 0xE0) {
            unsigned char ext_scancode;
            do {
                status = inb(KBD_STATUS);
            } while (!(status & 1));
            ext_scancode = inb(KBD_DATA);

            if (!(ext_scancode & 0x80)) {
                switch (ext_scancode) {
                    case SC_ARROW_UP: return CHAR_ARROW_UP;
                    case SC_ARROW_DOWN: return CHAR_ARROW_DOWN;
                    case SC_ARROW_LEFT: return CHAR_ARROW_LEFT;
                    case SC_ARROW_RIGHT: return CHAR_ARROW_RIGHT;
                }
            }
            return 0; 
        }
        
        if(scancode == 0x2A || scancode == 0x36) { 
            shift_pressed = true;
            return 0; 
        }
        if(scancode == (0x2A + 0x80) || scancode == (0x36 + 0x80)) { 
            shift_pressed = false;
            return 0;
        }

        if(scancode < 128) {
            if (scancode >= 0x80) { 
                return 0; 
            }
            if(shift_pressed) {
                return scancode_to_shifted_ascii[scancode];
            } else {
                return scancode_to_ascii[scancode];
            }
        }
    }
    return 0;
}