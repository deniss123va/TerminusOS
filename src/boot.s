.section .multiboot
.align 4
.long 0x1BADB002
.long 0x00
.long -(0x1BADB002+0x00)

.global _start
.section .text
_start:
    cli
    call kmain
    hlt
