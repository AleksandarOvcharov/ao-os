; Interrupt handling for AO OS
section .text

global isr_stub_table
global idt_load

extern timer_tick

; Timer interrupt handler (IRQ0)
global irq0_handler
irq0_handler:
    pusha
    
    extern timer_tick
    call timer_tick
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popa
    iret

; Load IDT
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

section .data
isr_stub_table:
    dd irq0_handler
