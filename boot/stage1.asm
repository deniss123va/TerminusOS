; ============================================================
; stage1.asm — TerminusOS MBR / El-Torito Stage 1
; ============================================================
; Собрать (build.sh делает это сам):  nasm -f bin stage1.asm -o stage1.bin
; Размер:   ровно 512 байт
;
; Работает в ДВУХ сценариях с одним и тем же бинарником:
;
;   A) HDD/USB прямая загрузка (disk.img записан через dd):
;      BIOS грузит ТОЛЬКО этот сектор (512 байт) на 0x7C00.
;      Код сам читает Stage 2 с диска (LBA/CHS) в память 0x7E00.
;
;   B) ISO/CD-ROM через El-Torito "no emulation boot":
;      BIOS грузит ВЕСЬ disk.img (через -boot-load-size) единым
;      куском начиная с 0x7C00 — Stage 2 и ядро уже лежат в RAM,
;      никаких дисковых вызовов не требуется вообще.
;
;   Различить сценарии позволяет 4-байтная метка 0xDEADC0DE в
;   самом начале stage2.bin: если она уже видна по адресу 0x7E00
;   сразу после старта — значит мы в сценарии (B) и просто прыгаем
;   дальше; если там мусор — мы в сценарии (A) и читаем с диска.
;
; Раскладка диска:
;   Сектор  0      : stage1.bin  (этот файл)
;   Секторы 1-65    : stage2.bin  (до 65*512 = 33280 байт, с запасом)
;   Сектор  66+     : kernel.elf  (0x7C00 + 66*512 = 0x10000 ровно —
;                      тот же адрес, куда Stage 2 сам кладёт ядро)
; ============================================================

    [bits 16]
    [org 0x7C00]

%define STAGE2_ADDR 0x7E00      ; 0x7C00 + 512 (сразу после Stage 1)
%define STAGE2_MAGIC 0xDEADC0DE
%define STAGE2_SECTORS 65       ; Сколько секторов резервируем под Stage 2

_start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl        ; BIOS передаёт номер диска в DL

    mov si, msg_boot
    call puts

; --- Проверка: Stage 2 уже в памяти? (сценарий B — ISO no-emul) -----
    cmp dword [STAGE2_ADDR], STAGE2_MAGIC
    je .jump_to_stage2          ; Уже преднагружено BIOS — диск не трогаем

; --- Сценарий A: грузим Stage 2 сами (HDD/USB) -----------------------
    call detect_ext

    cmp byte [has_ext], 0
    je .chs_path

; ---- Путь 1: Extended LBA Read (быстрый, одним вызовом) ----
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .disk_err
    jmp .jump_to_stage2

; ---- Путь 2: CHS Read (по сектору, для совместимости) -------
.chs_path:
    mov word [cur_lba], 1
    mov word [buf_off], STAGE2_ADDR
    mov cx, STAGE2_SECTORS       ; Читаем все зарезервированные секторы
                                 ; Stage 2, чтобы гарантированно
                                 ; вместить stage2.bin целиком.

.chs_loop:
    push cx
    call lba_to_chs             ; -> ch=cyl, cl=sector, dh=head

    mov ax, 0x0201               ; AH=02 (read), AL=1 сектор
    mov bx, [buf_off]
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    add word [buf_off], 512
    inc word [cur_lba]
    pop cx
    loop .chs_loop

.jump_to_stage2:
    jmp 0x0000:(STAGE2_ADDR + 4)  ; +4 — пропустить 4-байтную метку

.disk_err:
    mov si, msg_err
    call puts
    cli
    hlt

; --- Определить поддержку расширений int13h, иначе геометрию --------
detect_ext:
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .no_ext
    cmp bx, 0xAA55
    jne .no_ext

    mov byte [has_ext], 1
    ret

.no_ext:
    mov byte [has_ext], 0

    push es
    push di
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    pop di
    pop es
    jc .geom_fallback

    mov al, cl
    and al, 0x3F                ; CL[5:0] = sectors per track
    test al, al
    jz .geom_fallback            ; BIOS вернул 0 -> мусор, не доверяем
    movzx ax, al
    mov [spt], ax

    movzx ax, dh                ; DH = max head index (0-based)
    inc ax
    mov [heads], ax
    ret

.geom_fallback:
    mov word [spt], 63          ; Стандартная "безопасная" геометрия
    mov word [heads], 16
    ret

; --- LBA -> CHS (для маленьких LBA, cylinder < 256) ------------------
; in:  [cur_lba]
; out: ch=cylinder(low8), cl=sector(1-63), dh=head
lba_to_chs:
    push ax
    push bx

    mov ax, [cur_lba]
    xor dx, dx
    mov bx, [spt]
    div bx                       ; ax = lba/spt, dx = lba%spt
    inc dx
    mov cl, dl                   ; sector (1-based)

    xor dx, dx
    mov bx, [heads]
    div bx                       ; ax = cylinder, dx = head
    mov ch, al
    mov dh, dl

    pop bx
    pop ax
    ret

; --- puts: вывести строку через BIOS ----------------------------------
puts:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp puts
.done:
    ret

; --- Данные ------------------------------------------------------------
msg_boot    db "TerminusOS MBR", 0x0D, 0x0A, 0
msg_err     db "Disk error!", 0x0D, 0x0A, 0
boot_drive  db 0x80
has_ext     db 0
spt         dw 63
heads       dw 16
cur_lba     dw 0
buf_off     dw 0

; --- Disk Address Packet (для Extended LBA Read) -----------------------
align 4
dap:
    db 0x10                 ; Размер DAP
    db 0x00                 ; Зарезервировано
    dw STAGE2_SECTORS       ; Секторов читать (весь зарезервированный Stage 2)
    dw STAGE2_ADDR  ; Буфер: смещение
    dw 0x0000       ; Буфер: сегмент
    dq 1            ; LBA начало (сразу после MBR)

; --- Подпись MBR ----------------------------------------------------------
    times 510-($-$$) db 0
    dw 0xAA55
