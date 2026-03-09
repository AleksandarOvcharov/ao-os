; AO OS Stage 1 Bootloader - MBR (512 bytes)
; Loads stage2 from sectors 2-5 to 0x7E00 and jumps there.
; BIOS loads this at 0x7C00.

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save boot drive number
    mov [boot_drive], dl

    ; Print loading message
    mov si, msg_loading
    call print_str

    ; Load stage2: sectors 2..5 (4 sectors) to 0x0000:0x7E00
    mov ah, 0x02        ; BIOS read sectors
    mov al, 4           ; number of sectors to read
    mov ch, 0           ; cylinder 0
    mov cl, 2           ; starting sector 2 (1-based)
    mov dh, 0           ; head 0
    mov dl, [boot_drive]
    mov bx, 0x7E00      ; load to ES:BX = 0000:7E00
    int 0x13
    jc  disk_error

    ; Jump to stage2
    jmp 0x0000:0x7E00

disk_error:
    mov si, msg_disk_err
    call print_str
.halt:
    cli
    hlt
    jmp .halt

; Print null-terminated string in SI
print_str:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0x00
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

boot_drive: db 0

msg_loading:  db "AO Bootloader Stage1...", 0x0D, 0x0A, 0
msg_disk_err: db "Disk read error!", 0x0D, 0x0A, 0

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
