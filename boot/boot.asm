; AO OS Kernel Entry Point
; Entered from the custom bootloader in 32-bit protected mode.
; ESP is already set by stage2; we just call kernel_main.

[BITS 32]

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
_start:
    mov esp, stack_top

    extern kernel_main
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang
