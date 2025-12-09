#include "panic.h"
#include "screen.h" // Для println, video_memory, update_vga_cursor

extern "C" {
    /**
     * @brief Критическая остановка ядра.
     * Выводит сообщение о панике (белый текст на красном фоне), 
     * отключает прерывания и останавливает процессор.
     */
    void panic(const char* message) {
        // Установка цвета (белый на красном)
        for(int i=0;i<80*25;i++) video_memory[i]=0x4F00; 
        cursor_pos = 0;
        update_vga_cursor(0, 0);

        println("********************************************************************************");
        println("*** KERNEL PANIC                                 ***");
        println("********************************************************************************");
        print("Message: ");
        println(message);
        println("System Halted.");
        
        __asm__ volatile ("cli"); // Отключаем прерывания
        while(1) {
            __asm__ volatile ("hlt"); // Останавливаем процессор
        }
    }
}