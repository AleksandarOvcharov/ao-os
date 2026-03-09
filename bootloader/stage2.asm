; AO OS Stage 2 Bootloader
; Loaded at 0x0000:0x7E00 by stage1.
;
; Strategy:
;   1. Enable A20
;   2. Enter unreal mode (load 4GB flat data descriptor into FS,
;      then drop back to real mode — FS keeps its 4GB limit)
;   3. Read kernel sectors (LBA 5+) in 32-sector batches using
;      INT 13h CHS (simple, works on all BIOS/QEMU)
;   4. Copy each batch from bounce buffer (0x9000) to dest (0x100000+)
;      using FS-relative 32-bit addressing with 'a32' prefix
;   5. Load GDT, enter 32-bit protected mode, jump to 0x100000

[BITS 16]
[ORG 0x7E00]

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_stage2
    call print16

    ; ── A20 via BIOS ────────────────────────────────────────────────
    mov ax, 0x2401
    int 0x15

    ; ── Unreal mode: give FS a 4 GB limit ───────────────────────────
    cli
    lgdt [gdt_ptr]

    mov eax, cr0
    or  al, 1
    mov cr0, eax            ; enter pmode briefly

    mov ax, 0x10            ; flat data selector
    mov fs, ax              ; FS now has 4 GB limit

    and al, 0xFE
    mov cr0, eax            ; back to real mode; FS limit is still 4 GB
    sti

    ; ── Read kernel from disk into 0x100000 ─────────────────────────
    ; Disk layout: LBA 0 = stage1, LBA 1-4 = stage2, LBA 5+ = kernel
    ; CHS for LBA on QEMU HDD (63 spt, 255 heads):
    ;   C = LBA / (255*63),  H = (LBA / 63) % 255,  S = (LBA % 63)+1

    mov dword [lba_cur],  5         ; first kernel LBA
    mov dword [dest],     0x100000  ; load to 1 MB
    mov word  [remaining], KERNEL_SECS

.batch:
    cmp word [remaining], 0
    je  .done_read

    ; sectors this batch: min(remaining, 32)
    mov ax, [remaining]
    cmp ax, 32
    jbe .got_batch
    mov ax, 32
.got_batch:
    mov [batch], ax

    ; ── LBA → CHS ───────────────────────────────────────────────────
    ; Using 63 spt / 255 heads (standard BIOS geometry for raw HDD)
    mov eax, [lba_cur]
    xor edx, edx
    mov ecx, 63
    div ecx                 ; eax = LBA/63, edx = sector-1 (0-based)
    mov byte [s], dl        ; S = (LBA % 63) + 1 — add 1 below
    inc byte [s]

    xor edx, edx
    mov ecx, 255
    div ecx                 ; eax = cylinder, edx = head
    mov [h], dl
    mov [c], ax             ; cylinder (fits in 10 bits for BIOS)

    ; ── INT 13h read to bounce buffer at 0x0900:0x0000 = linear 0x9000
    ; Use segment:offset form so ES:BX = 0x0900:0x0000
    push es
    mov  ax, 0x0900
    mov  es, ax
    xor  bx, bx

    mov  ah, 0x02
    mov  al, [batch]
    mov  ch, [c]             ; cylinder low 8 bits
    mov  cl, [s]             ; sector (bits 0-5)
    ; merge cylinder high 2 bits into CL bits 6-7
    mov  bl, byte [c+1]
    and  bl, 0x03
    shl  bl, 6
    or   cl, bl
    xor  bx, bx              ; ES:BX = 0x0900:0x0000

    mov  dh, [h]
    mov  dl, [boot_drive]
    int  0x13
    pop  es                  ; restore ES (unreal mode data selector)
    jc   .disk_err

    ; ── Copy bounce buffer (linear 0x9000) → dest using FS ──────────
    movzx ecx, word [batch]
    shl   ecx, 9             ; * 512 = byte count
    mov   esi, 0x9000        ; source linear address
    mov   edi, [dest]        ; 32-bit destination (above 1 MB)

.copy:
    mov   al, [esi]
    a32   mov [fs:edi], al   ; FS has 4 GB limit from unreal mode
    inc   esi
    inc   edi
    dec   ecx
    jnz   .copy

    ; ── Advance counters ────────────────────────────────────────────
    movzx eax, word [batch]
    add   [lba_cur], eax     ; advance LBA
    shl   eax, 9
    add   [dest], eax        ; advance destination pointer
    mov   ax, [batch]
    sub   [remaining], ax    ; subtract sectors loaded
    jmp   .batch

.done_read:
    mov si, msg_jump
    call print16

    ; ── Enter 32-bit protected mode ─────────────────────────────────
    cli
    lgdt [gdt_ptr]

    mov eax, cr0
    or  al, 1
    mov cr0, eax

    jmp 0x08:pmode32        ; far jump flushes CS

.disk_err:
    mov si, msg_err
    call print16
.halt:
    cli
    hlt
    jmp .halt

; ── Real-mode print (SI = null-terminated string) ───────────────────
print16:
    push ax
    push bx
    mov  ah, 0x0E
    xor  bx, bx
.lp:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .lp
.done:
    pop bx
    pop ax
    ret

; ── Data ────────────────────────────────────────────────────────────
boot_drive: db 0
lba_cur:    dd 5
dest:       dd 0x100000
remaining:  dw KERNEL_SECS
batch:      dw 0
c:          dw 0
h:          db 0
s:          db 0

KERNEL_SECS equ 256         ; 256 * 512 = 128 KB max kernel

msg_stage2: db "Stage2 OK", 0x0D, 0x0A, 0
msg_jump:   db "Jumping to kernel...", 0x0D, 0x0A, 0
msg_err:    db "Disk error!", 0x0D, 0x0A, 0

; ── GDT ─────────────────────────────────────────────────────────────
align 8
gdt:
.null:  dq 0x0000000000000000
.code:  dq 0x00CF9A000000FFFF   ; 32-bit code, base=0, limit=4G
.data:  dq 0x00CF92000000FFFF   ; 32-bit data, base=0, limit=4G
gdt_end:

gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

; ── 32-bit protected mode entry ─────────────────────────────────────
[BITS 32]
pmode32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00200000     ; stack below 2 MB

    jmp 0x08:0x100000       ; jump to kernel at 1 MB

; Pad to exactly 4 sectors (2048 bytes)
times 2048 - ($ - $$) db 0
