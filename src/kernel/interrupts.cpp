#include "interrupts.h"
#include "screen.h" 
#include "panic.h" 
#include "../drivers/disk.h" // Для inb, outb

// IDT
idt_entry idt[256];
idt_ptr idtp;

// Коды ошибок для исключений
const char *exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

// **********************************************
// ===== Объявления обработчиков из ассемблера (ISR/IRQ) - ИСПРАВЛЕНО =====
// **********************************************
extern "C" {
    // 32 Обработчика исключений (ISR 0-31)
    void isr0(); void isr1(); void isr2(); void isr3(); void isr4(); void isr5();
    void isr6(); void isr7(); void isr8(); void isr9(); void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15(); void isr16(); void isr17();
    void isr18(); void isr19(); void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
    void isr30(); void isr31(); 
    
    // 16 Обработчиков прерываний (IRQ 0-15 -> INT 32-47)
    void irq0(); void irq1(); void irq2(); void irq3(); void irq4(); void irq5(); 
    void irq6(); void irq7(); void irq8(); void irq9(); void irq10(); void irq11(); 
    void irq12(); void irq13(); void irq14(); void irq15(); 

    // Внешняя функция на ассемблере для загрузки IDT
    void idt_flush(uint32_t idt_ptr_base);
}
// **********************************************
// ===== IDT Initialization and Gates =====
// **********************************************

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].flags = flags; 
}

// Переназначение PIC
void irq_remap() {
    #define PIC1_COMMAND    0x20
    #define PIC1_DATA       0x21
    #define PIC2_COMMAND    0xA0
    #define PIC2_DATA       0xA1
    
    // Инициализация (ICW1)
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    
    // Смещение вектора (ICW2): IRQ 0-7 -> INT 32-39, IRQ 8-15 -> INT 40-47
    outb(PIC1_DATA, 0x20); 
    outb(PIC2_DATA, 0x28); 
    
    // Подключение (ICW3)
    outb(PIC1_DATA, 0x04); 
    outb(PIC2_DATA, 0x02); 
    
    // Режим работы (ICW4)
    outb(PIC1_DATA, 0x01); 
    outb(PIC2_DATA, 0x01); 
    
    // Маскировка (OCW1): Отключаем все IRQ
    outb(PIC1_DATA, 0xFF); 
    outb(PIC2_DATA, 0xFF); 

    // Включаем только IRQ 1 (клавиатура)
    irq_clear_mask(1);
}

void init_idt() {
    idtp.limit = (sizeof(idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    // Очистка
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0; idt[i].offset_high = 0;
        idt[i].selector = 0; idt[i].zero = 0; idt[i].flags = 0;
    }

    // Установка ворот для исключений (ISR 0-31)
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E); 
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E); // General Protection Fault
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); // Page Fault
    // ... здесь должны быть все остальные isr
    
    // Установка ворот для IRQs (IRQ 0-15 -> INT 32-47)
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E); // IRQ 0 (Timer)
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E); // IRQ 1 (Keyboard)
    // ... здесь должны быть все остальные irq

    // Переназначение PIC
    irq_remap();
    
    // Включение прерываний и загрузка IDT
    __asm__ volatile ("sti");
    idt_flush((uint32_t)&idtp);
}

// **********************************************
// ===== Обработчики =====
// **********************************************

// Обработчик исключений (0-31)
extern "C" void isr_handler(registers regs) {
    if (regs.int_no < 32) {
        print("EXCEPTION: ");
        print(exception_messages[regs.int_no]);
        
        // Показать EIP (указатель инструкции)
        print(" (EIP: ");
        print_hex_byte((regs.eip >> 24) & 0xFF);
        print_hex_byte((regs.eip >> 16) & 0xFF);
        print_hex_byte((regs.eip >> 8) & 0xFF);
        print_hex_byte(regs.eip & 0xFF);
        println(")");
        
        panic(exception_messages[regs.int_no]);
    }
}

// Обработчик прерываний IRQ (32-47)
extern "C" void irq_handler(registers regs) {
    #define PIC_EOI 0x20
    
    // Отправка EOI (End of Interrupt) на Slave, если IRQ 8-15
    if (regs.int_no >= 40) { 
        outb(0xA0, PIC_EOI);
    }
    // Отправка EOI на Master
    outb(0x20, PIC_EOI); 

    switch (regs.int_no) {
        case 33: // IRQ 1: Keyboard
            // Аппаратный обработчик IRQ 1 позволяет считывать scancode в main loop.
            break;
    }
}

void irq_set_mask(uint8_t irq_line) {
    uint16_t port = (irq_line < 8) ? 0x21 : 0xA1;
    uint8_t value = inb(port) | (1 << (irq_line % 8));
    outb(port, value);        
}

void irq_clear_mask(uint8_t irq_line) {
    uint16_t port = (irq_line < 8) ? 0x21 : 0xA1;
    uint8_t value = inb(port) & ~(1 << (irq_line % 8));
    outb(port, value);        
}