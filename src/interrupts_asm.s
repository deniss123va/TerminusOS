# src/interrupts_asm.s
.section .text
.code32

# ********************************************************
# *********** Объявления глобальных символов ************
# ********************************************************
.global idt_flush 
.extern isr_handler
.extern irq_handler

# Объявления всех ISR (0-31)
.global isr0
.global isr1
.global isr2
.global isr3
.global isr4
.global isr5
.global isr6
.global isr7
.global isr8
.global isr9
.global isr10
.global isr11
.global isr12
.global isr13
.global isr14
.global isr15
.global isr16
.global isr17
.global isr18
.global isr19
.global isr20
.global isr21
.global isr22
.global isr23
.global isr24
.global isr25
.global isr26
.global isr27
.global isr28
.global isr29
.global isr30
.global isr31

# Объявления всех IRQ (0-15)
.global irq0
.global irq1
.global irq2
.global irq3
.global irq4
.global irq5
.global irq6
.global irq7
.global irq8
.global irq9
.global irq10
.global irq11
.global irq12
.global irq13
.global irq14
.global irq15

# ********************************************************
# ************ Код функций IDT и обработчиков ************
# ********************************************************

# IDT FLUSH (загрузка регистра IDTR)
.type idt_flush, @function
idt_flush:
    mov 4(%esp), %eax
    lidtl (%eax)
    ret

# ==================================
# ISRs БЕЗ КОДА ОШИБКИ (0-7, 9, 15, 16, 18-31)
# ==================================

# ISR 0
.type isr0, @function
isr0: 
    push $0
    pusha
    push $0 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 1
.type isr1, @function
isr1: 
    push $0
    pusha
    push $1 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 2
.type isr2, @function
isr2: 
    push $0
    pusha
    push $2
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 3
.type isr3, @function
isr3: 
    push $0
    pusha
    push $3
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 4
.type isr4, @function
isr4: 
    push $0
    pusha
    push $4
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 5
.type isr5, @function
isr5: 
    push $0
    pusha
    push $5
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 6
.type isr6, @function
isr6: 
    push $0
    pusha
    push $6
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 7
.type isr7, @function
isr7: 
    push $0
    pusha
    push $7
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 9
.type isr9, @function
isr9: 
    push $0
    pusha
    push $9
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 15
.type isr15, @function
isr15: 
    push $0
    pusha
    push $15 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 16
.type isr16, @function
isr16: 
    push $0
    pusha
    push $16 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 18
.type isr18, @function
isr18: 
    push $0
    pusha
    push $18
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 19
.type isr19, @function
isr19: 
    push $0
    pusha
    push $19 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 20
.type isr20, @function
isr20: 
    push $0
    pusha
    push $20 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 21
.type isr21, @function
isr21: 
    push $0
    pusha
    push $21 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 22
.type isr22, @function
isr22: 
    push $0
    pusha
    push $22 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 23
.type isr23, @function
isr23: 
    push $0
    pusha
    push $23 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 24
.type isr24, @function
isr24: 
    push $0
    pusha
    push $24 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 25
.type isr25, @function
isr25: 
    push $0
    pusha
    push $25 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 26
.type isr26, @function
isr26: 
    push $0
    pusha
    push $26 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 27
.type isr27, @function
isr27: 
    push $0
    pusha
    push $27 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 28
.type isr28, @function
isr28: 
    push $0
    pusha
    push $28 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 29
.type isr29, @function
isr29: 
    push $0
    pusha
    push $29 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret
    
# ISR 30
.type isr30, @function
isr30: 
    push $0
    pusha
    push $30 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 31
.type isr31, @function
isr31: 
    push $0
    pusha
    push $31 
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret


# ==================================
# ISRs С КОДОМ ОШИБКИ (8, 10-14, 17)
# ==================================
# Общий шаблон: pusha, push $INT_NO, call isr_handler, pop %eax, popa, add $8, %esp, iret

# ISR 8
.type isr8, @function
isr8: 
    pusha
    push $8
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 10
.type isr10, @function
isr10: 
    pusha
    push $10
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 11
.type isr11, @function
isr11: 
    pusha
    push $11
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 12
.type isr12, @function
isr12: 
    pusha
    push $12
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 13
.type isr13, @function
isr13: 
    pusha
    push $13
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 14
.type isr14, @function
isr14: 
    pusha
    push $14
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret

# ISR 17
.type isr17, @function
isr17: 
    pusha
    push $17
    call isr_handler
    pop %eax
    popa
    add $8, %esp
    iret


# ==================================
# IRQs (32-47)
# ==================================
# Общий шаблон: push $0, pusha, push $INT_NO, call irq_handler, pop %eax, popa, add $8, %esp, iret

# IRQ 0 -> INT 32
.type irq0, @function
irq0: 
    push $0
    pusha
    push $32
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 1 -> INT 33
.type irq1, @function
irq1: 
    push $0
    pusha
    push $33
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 2 -> INT 34
.type irq2, @function
irq2: 
    push $0
    pusha
    push $34
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 3 -> INT 35
.type irq3, @function
irq3: 
    push $0
    pusha
    push $35
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 4 -> INT 36
.type irq4, @function
irq4: 
    push $0
    pusha
    push $36
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 5 -> INT 37
.type irq5, @function
irq5: 
    push $0
    pusha
    push $37
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 6 -> INT 38
.type irq6, @function
irq6: 
    push $0
    pusha
    push $38
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 7 -> INT 39
.type irq7, @function
irq7: 
    push $0
    pusha
    push $39
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 8 -> INT 40
.type irq8, @function
irq8: 
    push $0
    pusha
    push $40
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 9 -> INT 41
.type irq9, @function
irq9: 
    push $0
    pusha
    push $41
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 10 -> INT 42
.type irq10, @function
irq10: 
    push $0
    pusha
    push $42
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 11 -> INT 43
.type irq11, @function
irq11: 
    push $0
    pusha
    push $43
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 12 -> INT 44
.type irq12, @function
irq12: 
    push $0
    pusha
    push $44
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 13 -> INT 45
.type irq13, @function
irq13: 
    push $0
    pusha
    push $45
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 14 -> INT 46
.type irq14, @function
irq14: 
    push $0
    pusha
    push $46
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret

# IRQ 15 -> INT 47
.type irq15, @function
irq15: 
    push $0
    pusha
    push $47
    call irq_handler
    pop %eax
    popa
    add $8, %esp
    iret