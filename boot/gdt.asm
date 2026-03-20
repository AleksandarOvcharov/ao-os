; AO OS - Global Descriptor Table (64-bit long mode)
; Provides kernel code/data segments and a TSS for interrupt stack switching.
[BITS 64]

section .data

; ─── GDT entries ─────────────────────────────────────────────────────────────
; 64-bit long mode GDT: most fields are ignored, but the structure must exist.
; Entry 0: Null descriptor (required)
; Entry 1: Kernel code segment (selector 0x08)
; Entry 2: Kernel data segment (selector 0x10)
; Entry 3: TSS descriptor (selector 0x18, 16 bytes wide)

global gdt_start
global gdt_end
global gdt_descriptor
global tss

align 16
gdt_start:
    ; Entry 0: Null descriptor
    dq 0x0000000000000000

    ; Entry 1: Kernel Code Segment (selector 0x08)
    ; Base=0, Limit=0xFFFFF, Access=0x9A (present, ring 0, code, exec/read)
    ; Flags=0xA (64-bit, 4K granularity)
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base mid
    db 10011010b    ; Access: P=1, DPL=00, S=1, Type=1010 (code, exec/read)
    db 10101111b    ; Flags=1010 (G=1, L=1), Limit high=1111
    db 0x00         ; Base high

    ; Entry 2: Kernel Data Segment (selector 0x10)
    ; Base=0, Limit=0xFFFFF, Access=0x92 (present, ring 0, data, read/write)
    ; Flags=0xC (32-bit compatible, 4K granularity)
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base mid
    db 10010010b    ; Access: P=1, DPL=00, S=1, Type=0010 (data, read/write)
    db 11001111b    ; Flags=1100 (G=1, D/B=1), Limit high=1111
    db 0x00         ; Base high

    ; Entry 3: TSS descriptor (selector 0x18) - 16 bytes in 64-bit mode
    ; Will be filled in at runtime by C code (tss_init)
    global gdt_tss_entry
    gdt_tss_entry:
    dw 0            ; Limit low (set by C)
    dw 0            ; Base low (set by C)
    db 0            ; Base mid-low (set by C)
    db 10001001b    ; Access: P=1, DPL=00, S=0, Type=1001 (64-bit TSS available)
    db 0            ; Flags + Limit high (set by C)
    db 0            ; Base mid-high (set by C)
    dd 0            ; Base high (set by C)
    dd 0            ; Reserved
gdt_end:

; GDT descriptor (pointer for lgdt)
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Limit (size - 1)
    dq gdt_start                 ; Base address

; ─── Task State Segment ──────────────────────────────────────────────────────
; 104 bytes, used for interrupt stack switching in 64-bit mode.
; RSP0 = stack pointer to use when transitioning from ring 3 to ring 0.
; IST1-7 = Interrupt Stack Table entries for dedicated stacks.

align 16
tss:
    dd 0            ; Reserved
    ; RSP0 - RSP2 (stack pointers for privilege levels 0-2)
    dq 0            ; RSP0 (offset 4) - set by C code
    dq 0            ; RSP1 (offset 12)
    dq 0            ; RSP2 (offset 20)
    dq 0            ; Reserved (offset 28)
    ; IST1-IST7 (Interrupt Stack Table)
    dq 0            ; IST1 (offset 36) - used for double fault
    dq 0            ; IST2 (offset 44)
    dq 0            ; IST3 (offset 52)
    dq 0            ; IST4 (offset 60)
    dq 0            ; IST5 (offset 68)
    dq 0            ; IST6 (offset 76)
    dq 0            ; IST7 (offset 84)
    dq 0            ; Reserved (offset 92)
    dw 0            ; Reserved (offset 100)
    dw 104          ; I/O Map Base Address (offset 102) = size of TSS (no I/O bitmap)
