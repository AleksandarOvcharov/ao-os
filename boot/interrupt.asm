; Interrupt handling for AO OS (64-bit long mode)
[BITS 64]
section .text

global isr_stub_table
global idt_load

extern timer_tick

; Timer interrupt handler (IRQ0)
global irq0_handler
irq0_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11

    call timer_tick

    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al

    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; Syscall handler (int 0x80)
; rax = syscall number
; rbx = arg1, rcx = arg2, rdx = arg3
; return value in rax
global syscall_handler
extern syscall_dispatch
syscall_handler:
    ; Save all registers we'll clobber
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11

    ; Call syscall_dispatch(num, arg1, arg2, arg3)
    ; System V AMD64 ABI: rdi, rsi, rdx, rcx
    mov rdi, rax        ; 1st param: syscall number
    mov rsi, rbx        ; 2nd param: arg1
    mov r8,  rdx        ; save arg3 (rdx will be overwritten)
    mov rdx, rcx        ; 3rd param: arg2
    mov rcx, r8         ; 4th param: arg3

    call syscall_dispatch
    ; rax now holds return value — we keep it

    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    ; rax is NOT restored — it carries the syscall return value
    iretq

; Load IDT
idt_load:
    lidt [rdi]
    ret

section .data
isr_stub_table:
    dq irq0_handler
