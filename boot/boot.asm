; AO OS Kernel Entry Point
; Entered from the custom bootloader in 64-bit long mode.
; RSP is already set by stage2; we just call kernel_main.

[BITS 64]

section .bss
align 16
stack_bottom:
    resb 16384
global stack_top
stack_top:

section .text
global _start
_start:
    mov rsp, stack_top

    extern kernel_main
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang
