; Interrupt handling for AO OS (64-bit long mode)
; Includes: CPU exception handlers (ISR 0-21), IRQ handlers, syscall, GDT
[BITS 64]
section .text

global idt_load
global gdt_load

; ─── CPU Exception Handlers (ISR 0-21) ───────────────────────────────────────
; Some exceptions push an error code, some don't.
; We push a dummy 0 for those that don't, so the stack frame is uniform.

; Macro for exceptions WITHOUT error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0           ; dummy error code
    push qword %1          ; interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exceptions WITH error code (CPU pushes it automatically)
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1          ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; ISR 0-21: CPU Exceptions
ISR_NOERR 0     ; Division by Zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; Non-Maskable Interrupt
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound Range Exceeded
ISR_NOERR 6     ; Invalid Opcode
ISR_NOERR 7     ; Device Not Available
ISR_ERR   8     ; Double Fault
ISR_NOERR 9     ; Coprocessor Segment Overrun (legacy)
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment Not Present
ISR_ERR   12    ; Stack-Segment Fault
ISR_ERR   13    ; General Protection Fault
ISR_ERR   14    ; Page Fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 Floating-Point Exception
ISR_ERR   17    ; Alignment Check
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD Floating-Point Exception
ISR_NOERR 20    ; Virtualization Exception
ISR_ERR   21    ; Control Protection Exception

; ─── Common ISR stub ─────────────────────────────────────────────────────────
; Stack at entry: [SS, RSP, RFLAGS, CS, RIP, error_code, int_number]
; We save all general-purpose registers and pass a pointer to the C handler.

extern exception_handler

isr_common_stub:
    ; Save all general-purpose registers
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
    push r12
    push r13
    push r14
    push r15

    ; Read CR2 (page fault address) - safe to read even for non-page-faults
    mov rax, cr2
    push rax

    ; Pass pointer to this register frame as argument to C handler
    mov rdi, rsp
    call exception_handler

    ; Pop CR2
    add rsp, 8

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
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

    ; Remove interrupt number and error code from stack
    add rsp, 16

    iretq

; ─── IRQ Handlers ────────────────────────────────────────────────────────────

; Timer interrupt handler (IRQ0 - mapped to INT 32)
extern timer_tick
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

    ; Send EOI to master PIC
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

; Keyboard interrupt handler (IRQ1 - mapped to INT 33)
extern keyboard_irq_handler
global irq1_handler
irq1_handler:
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

    call keyboard_irq_handler

    ; Send EOI to master PIC
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

; ─── Syscall handler (int 0x80) ─────────────────────────────────────────────
; rax = syscall number
; rbx = arg1, rcx = arg2, rdx = arg3
; return value in rax
global syscall_handler
extern syscall_dispatch
syscall_handler:
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

    ; System V AMD64 ABI: rdi, rsi, rdx, rcx
    mov rdi, rax        ; 1st param: syscall number
    mov rsi, rbx        ; 2nd param: arg1
    mov r8,  rdx        ; save arg3
    mov rdx, rcx        ; 3rd param: arg2
    mov rcx, r8         ; 4th param: arg3

    call syscall_dispatch
    ; rax now holds return value

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

; ─── IDT / GDT load functions ───────────────────────────────────────────────

idt_load:
    lidt [rdi]
    ret

; Load a new GDT and reload segment registers
; rdi = pointer to GDT descriptor (limit + base)
gdt_load:
    lgdt [rdi]
    ; Reload CS via a far return
    push 0x08               ; kernel code segment selector
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    ; Reload data segment registers
    mov ax, 0x10            ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; ─── ISR stub table (for C code to reference) ───────────────────────────────

section .data
global isr_stub_table
isr_stub_table:
    dq isr0
    dq isr1
    dq isr2
    dq isr3
    dq isr4
    dq isr5
    dq isr6
    dq isr7
    dq isr8
    dq isr9
    dq isr10
    dq isr11
    dq isr12
    dq isr13
    dq isr14
    dq isr15
    dq isr16
    dq isr17
    dq isr18
    dq isr19
    dq isr20
    dq isr21
