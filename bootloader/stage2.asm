; AO OS Stage 2 Bootloader
; Boot flag @ 0x7000: 0x00=normal boot, 0x01=reinstall
;
; VGA direct-write layout (0xB800):
;   Each cell = [char][attr], row*80+col)*2
;   row*80 = (row<<6)+(row<<4)  — no imul needed
;
; Colour palette:
;   0x17 = white on blue        (background)
;   0x07 = dark white on black  (dim)
;   0x1F = bright white on blue (normal item)
;   0x70 = black on light grey  (normal item alt)
;   0x1E = yellow on blue       (selected row)
;   0x3F = bright white on cyan (selected row alt)
;   0x4F = white on red         (warning/error)
;   0x4E = yellow on red        (warning title)
;   0x2F = bright white on green (Yes selected)
;   0x1A = bright green on blue (Yes normal)

%define VGA_SEG  0xB800

; ── Macro: compute VGA offset DI = (row*80 + col)*2 ─────────────────
; row in BH, col in BL. Uses AX as scratch, trashes nothing else.
%macro VGA_OFFSET 0
    push ax
    xor  ax, ax
    mov  al, bh          ; ax = row
    mov  di, ax
    shl  di, 6           ; di = row*64
    shl  ax, 4           ; ax = row*16
    add  di, ax          ; di = row*80
    xor  ah, ah
    mov  al, bl
    add  di, ax          ; di = row*80+col
    shl  di, 1           ; *2 = byte offset
    pop  ax
%endmacro

[BITS 16]
[ORG 0x7E00]

stage2_main:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti
    mov  [boot_drive], dl

    ; ── A20 ──────────────────────────────────────────────────────────
    mov  ax, 0x2401
    int  0x15

    ; ── Unreal mode: FS = 4 GB flat ──────────────────────────────────
    cli
    lgdt [gdt_ptr]
    mov  eax, cr0
    or   al, 1
    mov  cr0, eax
    mov  ax, 0x10
    mov  fs, ax
    and  al, 0xFE
    mov  cr0, eax
    sti

    ; ── VGA mode 3, hide cursor ───────────────────────────────────────
    mov  ax, 0x0003
    int  0x10
    mov  ah, 0x01
    mov  cx, 0x2000        ; hide cursor (start > end)
    int  0x10

    ; ── Draw UI, start menu loop ──────────────────────────────────────
    call ui_draw_main
    mov  byte [sel], 0
    call ui_draw_items

.key_loop:
    xor  ah, ah
    int  0x16              ; AH=scancode

    cmp  ah, 0x48          ; Up
    je   .up
    cmp  ah, 0x50          ; Down
    je   .down
    cmp  ah, 0x1C          ; Enter
    je   .enter
    jmp  .key_loop
.up:
    cmp  byte [sel], 0
    je   .key_loop
    dec  byte [sel]
    call ui_draw_items
    jmp  .key_loop
.down:
    cmp  byte [sel], 1
    je   .key_loop
    inc  byte [sel]
    call ui_draw_items
    jmp  .key_loop
.enter:
    cmp  byte [sel], 0
    je   .do_boot
    call ui_draw_confirm
    mov  byte [conf_sel], 1  ; default = No
    call ui_draw_yesno
.ck:
    xor  ah, ah
    int  0x16
    cmp  ah, 0x4B          ; Left
    je   .cl
    cmp  ah, 0x4D          ; Right
    je   .cr
    cmp  ah, 0x1C          ; Enter
    je   .ce
    cmp  ah, 0x01          ; Esc
    je   .cc
    jmp  .ck
.cl:
    cmp  byte [conf_sel], 0
    je   .ck
    dec  byte [conf_sel]
    call ui_draw_yesno
    jmp  .ck
.cr:
    cmp  byte [conf_sel], 1
    je   .ck
    inc  byte [conf_sel]
    call ui_draw_yesno
    jmp  .ck
.ce:
    cmp  byte [conf_sel], 0
    jne  .cc
    mov  byte [boot_flag], 0x01
    jmp  load_kernel
.cc:
    call ui_draw_main
    mov  byte [sel], 1
    call ui_draw_items
    jmp  .key_loop
.do_boot:
    mov  byte [boot_flag], 0x00
    jmp  load_kernel

; ════════════════════════════════════════════════════════════════════
; Primitives
; ════════════════════════════════════════════════════════════════════

; vga_putc: AL=char AH=attr BH=row BL=col
vga_putc:
    push es
    push di
    push bx
    push ax
    mov  di, VGA_SEG
    mov  es, di
    VGA_OFFSET               ; DI = byte offset
    pop  ax
    mov  [es:di], al         ; char
    mov  [es:di+1], ah       ; attr
    pop  bx
    pop  di
    pop  es
    ret

; vga_puts: SI=str AH=attr BH=row BL=col  (BL advances per char)
vga_puts:
    push si
    push ax
    push bx
.lp:
    lodsb
    test al, al
    jz   .done
    call vga_putc
    inc  bl
    jmp  .lp
.done:
    pop  bx
    pop  ax
    pop  si
    ret

; vga_fill_row: fill row BH from col BL for CX chars with AL=char AH=attr
vga_fill_row:
    push cx
    push bx
.lp:
    call vga_putc
    inc  bl
    loop .lp
    pop  bx
    pop  cx
    ret

; ════════════════════════════════════════════════════════════════════
; UI — Main menu
; Layout (80x25):
;   Row  0   : header bar  (full width, dark blue BG, cyan text)
;   Row  1   : blank
;   Row  2   : OS name large
;   Row  3   : version / tagline
;   Row  4   : blank
;   Row  5   : ┌─ box top ─────────────────────────────────────────┐
;   Row  6   : │  "Please select a boot option:"                   │
;   Row  7   : │ ──────────────────────────────────────────────── │
;   Row  8   : │  item 0                                           │
;   Row  9   : │                                                   │
;   Row 10   : │  item 1                                           │
;   Row 11   : │ ──────────────────────────────────────────────── │
;   Row 12   : └───────────────────────────────────────────────────┘
;   Row 24   : footer hint bar
; ════════════════════════════════════════════════════════════════════

ui_draw_main:
    push ax
    push bx
    push cx
    push si

    ; ── Fill whole screen dark blue ───────────────────────────────────
    push es
    push di
    mov  ax, VGA_SEG
    mov  es, ax
    xor  di, di
    mov  cx, 80*25
    mov  ax, 0x1720
    rep  stosw
    pop  di
    pop  es

    ; ── Header row 0: full-width cyan on dark blue ────────────────────
    mov  ah, 0x3F            ; bright white on cyan
    mov  al, ' '
    mov  bh, 0
    mov  bl, 0
    mov  cx, 80
    call vga_fill_row
    ; header text centred
    mov  ah, 0x3E            ; yellow on cyan
    mov  bh, 0
    mov  bl, 31
    mov  si, str_hdr
    call vga_puts

    ; ── OS title row 2 ───────────────────────────────────────────────
    mov  ah, 0x1F
    mov  bh, 2
    mov  bl, 37
    mov  si, str_os
    call vga_puts

    ; ── Tagline row 3 ────────────────────────────────────────────────
    mov  ah, 0x1B            ; cyan on blue
    mov  bh, 3
    mov  bl, 31
    mov  si, str_tag
    call vga_puts

    ; ── Box rows 5..12, cols 12..67 (width 56) ───────────────────────
    ; top ┌─...─┐
    mov  ah, 0x17
    mov  al, 0xDA            ; ┌
    mov  bh, 5
    mov  bl, 12
    call vga_putc
    mov  al, 0xC4            ; ─
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row
    mov  al, 0xBF            ; ┐
    mov  bl, 67
    call vga_putc

    ; sides rows 6..11
    mov  bh, 6
.sides:
    mov  ah, 0x17
    mov  al, 0xB3            ; │
    mov  bl, 12
    call vga_putc
    mov  al, ' '
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row
    mov  al, 0xB3
    mov  bl, 67
    call vga_putc
    inc  bh
    cmp  bh, 12
    jb   .sides

    ; bottom └─...─┘
    mov  ah, 0x17
    mov  al, 0xC0            ; └
    mov  bh, 12
    mov  bl, 12
    call vga_putc
    mov  al, 0xC4
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row
    mov  al, 0xD9            ; ┘
    mov  bl, 67
    call vga_putc

    ; prompt row 6
    mov  ah, 0x1B
    mov  bh, 6
    mov  bl, 14
    mov  si, str_prompt
    call vga_puts

    ; divider row 7
    mov  ah, 0x18
    mov  al, 0xC4
    mov  bh, 7
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row

    ; divider row 11
    mov  ah, 0x18
    mov  al, 0xC4
    mov  bh, 11
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row

    ; ── Footer row 24 ────────────────────────────────────────────────
    mov  ah, 0x70            ; black on light grey
    mov  al, ' '
    mov  bh, 24
    mov  bl, 0
    mov  cx, 80
    call vga_fill_row
    mov  ah, 0x70
    mov  bh, 24
    mov  bl, 20
    mov  si, str_footer
    call vga_puts

    pop  si
    pop  cx
    pop  bx
    pop  ax
    ret

; ui_draw_items: redraw the two selectable rows using [sel]
ui_draw_items:
    push ax
    push bx
    push cx
    push si

    ; ── Item 0 row 8 ─────────────────────────────────────────────────
    cmp  byte [sel], 0
    je   .i0hi
    mov  ah, 0x17            ; normal: white on blue
    jmp  .i0draw
.i0hi:
    mov  ah, 0x70            ; selected: black on light grey
.i0draw:
    mov  al, ' '
    mov  bh, 8
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row
    ; arrow indicator
    cmp  byte [sel], 0
    jne  .i0text
    mov  ah, 0x70
    mov  al, 0x10            ; ► (filled right arrow)
    mov  bh, 8
    mov  bl, 14
    call vga_putc
.i0text:
    mov  ah, 0x70
    cmp  byte [sel], 0
    je   .i0put
    mov  ah, 0x1F
.i0put:
    mov  bh, 8
    mov  bl, 16
    mov  si, str_opt0
    call vga_puts

    ; ── Item 1 row 10 ────────────────────────────────────────────────
    cmp  byte [sel], 1
    je   .i1hi
    mov  ah, 0x17
    jmp  .i1draw
.i1hi:
    mov  ah, 0x70
.i1draw:
    mov  al, ' '
    mov  bh, 10
    mov  bl, 13
    mov  cx, 54
    call vga_fill_row
    cmp  byte [sel], 1
    jne  .i1text
    mov  ah, 0x70
    mov  al, 0x10
    mov  bh, 10
    mov  bl, 14
    call vga_putc
.i1text:
    mov  ah, 0x70
    cmp  byte [sel], 1
    je   .i1put
    mov  ah, 0x1F
.i1put:
    mov  bh, 10
    mov  bl, 16
    mov  si, str_opt1
    call vga_puts

    pop  si
    pop  cx
    pop  bx
    pop  ax
    ret

; ════════════════════════════════════════════════════════════════════
; UI — Confirm reinstall screen
; ════════════════════════════════════════════════════════════════════
ui_draw_confirm:
    push ax
    push bx
    push cx
    push si

    ; Refill screen
    push es
    push di
    mov  ax, VGA_SEG
    mov  es, ax
    xor  di, di
    mov  cx, 80*25
    mov  ax, 0x1720
    rep  stosw
    pop  di
    pop  es

    ; ── Red warning header row 0 ─────────────────────────────────────
    mov  ah, 0x4F
    mov  al, ' '
    mov  bh, 0
    mov  bl, 0
    mov  cx, 80
    call vga_fill_row
    mov  ah, 0x4E
    mov  bh, 0
    mov  bl, 28
    mov  si, str_warn_hdr
    call vga_puts

    ; ── Warning icon + text rows 4,5,6 ───────────────────────────────
    mov  ah, 0x1E            ; yellow on blue
    mov  bh, 4
    mov  bl, 20
    mov  si, str_warn1
    call vga_puts
    mov  ah, 0x1F
    mov  bh, 5
    mov  bl, 20
    mov  si, str_warn2
    call vga_puts
    mov  ah, 0x1F
    mov  bh, 6
    mov  bl, 20
    mov  si, str_warn3
    call vga_puts

    ; ── Question row 9 ───────────────────────────────────────────────
    mov  ah, 0x1F
    mov  bh, 9
    mov  bl, 22
    mov  si, str_question
    call vga_puts

    ; ── Footer row 24 ────────────────────────────────────────────────
    mov  ah, 0x70
    mov  al, ' '
    mov  bh, 24
    mov  bl, 0
    mov  cx, 80
    call vga_fill_row
    mov  ah, 0x70
    mov  bh, 24
    mov  bl, 8
    mov  si, str_cfooter
    call vga_puts

    pop  si
    pop  cx
    pop  bx
    pop  ax
    ret

; ui_draw_yesno: draw Yes/No buttons at row 12, conf_sel=0→Yes
ui_draw_yesno:
    push ax
    push bx
    push si

    ; clear button row 12
    mov  ah, 0x17
    mov  al, ' '
    mov  bh, 12
    mov  bl, 0
    mov  cx, 80
    call vga_fill_row

    ; Yes button col 24
    cmp  byte [conf_sel], 0
    je   .yhi
    mov  ah, 0x1A            ; green on blue (normal)
    jmp  .ydr
.yhi:
    mov  ah, 0x2F            ; bright white on green (selected)
.ydr:
    mov  bh, 12
    mov  bl, 24
    mov  si, str_yes
    call vga_puts

    ; No button col 40
    cmp  byte [conf_sel], 1
    je   .nhi
    mov  ah, 0x1F            ; normal
    jmp  .ndr
.nhi:
    mov  ah, 0x4F            ; white on red (selected)
.ndr:
    mov  bh, 12
    mov  bl, 42
    mov  si, str_no
    call vga_puts

    pop  si
    pop  bx
    pop  ax
    ret

; ════════════════════════════════════════════════════════════════════
; Kernel loader
; ════════════════════════════════════════════════════════════════════
load_kernel:
    ; loading screen
    push es
    push di
    mov  ax, VGA_SEG
    mov  es, ax
    xor  di, di
    mov  cx, 80*25
    mov  ax, 0x1720
    rep  stosw
    pop  di
    pop  es
    mov  ah, 0x1F
    mov  bh, 12
    mov  bl, 31
    mov  si, msg_loading
    call vga_puts

    mov  dword [lba_cur],   5
    mov  dword [dest],      0x100000
    mov  word  [remaining], KERNEL_SECS

.batch:
    cmp  word [remaining], 0
    je   .done_read
    mov  ax, [remaining]
    cmp  ax, 32
    jbe  .got_batch
    mov  ax, 32
.got_batch:
    mov  [batch], ax

    mov  eax, [lba_cur]
    xor  edx, edx
    mov  ecx, 63
    div  ecx
    mov  byte [s], dl
    inc  byte [s]
    xor  edx, edx
    mov  ecx, 255
    div  ecx
    mov  [h], dl
    mov  [c], ax

    push es
    mov  ax, 0x0900
    mov  es, ax
    xor  bx, bx
    mov  ah, 0x02
    mov  al, [batch]
    mov  ch, [c]
    mov  cl, [s]
    mov  bl, byte [c+1]
    and  bl, 0x03
    shl  bl, 6
    or   cl, bl
    xor  bx, bx
    mov  dh, [h]
    mov  dl, [boot_drive]
    int  0x13
    pop  es
    jc   .disk_err

    movzx ecx, word [batch]
    shl   ecx, 9
    mov   esi, 0x9000
    mov   edi, [dest]
.copy:
    mov   al, [esi]
    a32   mov [fs:edi], al
    inc   esi
    inc   edi
    dec   ecx
    jnz   .copy

    movzx eax, word [batch]
    add   [lba_cur], eax
    shl   eax, 9
    add   [dest], eax
    mov   ax, [batch]
    sub   [remaining], ax
    jmp   .batch

.done_read:
    movzx eax, byte [boot_flag]
    a32   mov [fs:0x7000], al

    ; ── Enter 32-bit protected mode first ───────────────────────────
    cli
    lgdt [gdt_ptr]
    mov  eax, cr0
    or   al, 1
    mov  cr0, eax
    jmp  0x08:pmode32

.disk_err:
    mov  ah, 0x4F
    mov  bh, 14
    mov  bl, 29
    mov  si, msg_disk_err
    call vga_puts
.halt:
    cli
    hlt
    jmp  .halt

; ════════════════════════════════════════════════════════════════════
; Data
; ════════════════════════════════════════════════════════════════════
boot_drive:  db 0
boot_flag:   db 0
sel:         db 0
conf_sel:    db 1
lba_cur:     dd 5
dest:        dd 0x100000
remaining:   dw KERNEL_SECS
batch:       dw 0
c:           dw 0
h:           db 0
s:           db 0

KERNEL_SECS  equ 256

str_hdr:      db "AO OS  Bootloader", 0
str_os:       db "AO  OS", 0
str_tag:      db "v1.4.1  -  Aurora", 0
str_prompt:   db "Please select a boot option:", 0
str_opt0:     db "Boot AO OS normally", 0
str_opt1:     db "Reinstall  /  Format disk", 0
str_footer:   db "[ Up/Down ] Navigate    [ Enter ] Select", 0
str_warn_hdr: db "! WARNING !", 0
str_warn1:    db "All data on the primary disk will be ERASED.", 0
str_warn2:    db "The filesystem will be completely reformatted.", 0
str_warn3:    db "This action CANNOT be undone.", 0
str_question: db "Are you sure you want to reinstall?", 0
str_yes:      db "  YES - Reinstall  ", 0
str_no:       db "  NO  - Go Back  ", 0
str_cfooter:  db "[ Left/Right ] Navigate    [ Enter ] Confirm    [ Esc ] Cancel", 0
msg_loading:  db "Loading kernel...", 0
msg_disk_err: db "DISK READ ERROR!", 0

; ════════════════════════════════════════════════════════════════════
; GDT — used for initial 32-bit protected mode (unreal mode + PM entry)
; ════════════════════════════════════════════════════════════════════
align 8
gdt:
.null: dq 0x0000000000000000
.code: dq 0x00CF9A000000FFFF          ; 32-bit code, 4GB flat
.data: dq 0x00CF92000000FFFF          ; 32-bit data, 4GB flat
gdt_end:

gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

; ════════════════════════════════════════════════════════════════════
; 64-bit GDT — used for long mode
; ════════════════════════════════════════════════════════════════════
align 8
gdt64:
.null: dq 0x0000000000000000
.code: dq 0x00209A0000000000          ; 64-bit code: L=1, D=0, P=1, DPL=0
.data: dq 0x0000920000000000          ; 64-bit data: P=1, DPL=0, writable
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dd gdt64
    dd 0                              ; high 32 bits of base (for 64-bit lgdt)

; ════════════════════════════════════════════════════════════════════
; 32-bit protected mode: set up paging and switch to long mode
; ════════════════════════════════════════════════════════════════════
[BITS 32]
pmode32:
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  esp, 0x00200000

    ; ── Build identity-mapped page tables at 0x1000 ─────────────────
    ; PML4 @ 0x1000, PDPT @ 0x2000, PD0 @ 0x3000, PD1 @ 0x4000
    ; Maps first 4 GB using 2 MB huge pages (2 page directories × 512 entries)

    ; Zero out 4 pages (16 KB) for page tables
    mov  edi, 0x1000
    xor  eax, eax
    mov  ecx, 4096               ; 16384 bytes / 4
    rep  stosd

    ; PML4[0] → PDPT @ 0x2000 (present + writable)
    mov  dword [0x1000], 0x2003

    ; PDPT[0] → PD0 @ 0x3000 (present + writable)
    mov  dword [0x2000], 0x3003
    ; PDPT[1] → PD1 @ 0x4000 (present + writable)
    mov  dword [0x2008], 0x4003

    ; Fill PD0: 512 entries × 2 MB = first 1 GB
    mov  edi, 0x3000
    mov  eax, 0x0000_0083        ; present + writable + page size (2MB)
    mov  ecx, 512
.fill_pd0:
    mov  [edi], eax
    mov  dword [edi+4], 0        ; high 32 bits = 0
    add  eax, 0x00200000         ; next 2 MB
    add  edi, 8
    dec  ecx
    jnz  .fill_pd0

    ; Fill PD1: 512 entries × 2 MB = 1-2 GB
    mov  edi, 0x4000
    ; eax continues from 0x40000083 (1 GB mark)
    mov  ecx, 512
.fill_pd1:
    mov  [edi], eax
    mov  dword [edi+4], 0
    add  eax, 0x00200000
    add  edi, 8
    dec  ecx
    jnz  .fill_pd1

    ; ── Enable PAE (CR4 bit 5) ──────────────────────────────────────
    mov  eax, cr4
    or   eax, (1 << 5)
    mov  cr4, eax

    ; ── Load PML4 into CR3 ──────────────────────────────────────────
    mov  eax, 0x1000
    mov  cr3, eax

    ; ── Enable long mode (IA-32e) via EFER MSR ──────────────────────
    mov  ecx, 0xC0000080         ; IA32_EFER MSR
    rdmsr
    or   eax, (1 << 8)          ; LME = Long Mode Enable
    wrmsr

    ; ── Enable paging (CR0 bit 31) — this activates long mode ───────
    mov  eax, cr0
    or   eax, (1 << 31)
    mov  cr0, eax

    ; ── Load 64-bit GDT and far-jump to 64-bit code ────────────────
    lgdt [gdt64_ptr]
    jmp  0x08:lmode64

; ════════════════════════════════════════════════════════════════════
; 64-bit long mode entry
; ════════════════════════════════════════════════════════════════════
[BITS 64]
lmode64:
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  rsp, 0x00200000

    ; Jump to kernel at 1 MB
    jmp  0x100000

times 2048 - ($ - $$) db 0
