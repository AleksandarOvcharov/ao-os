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

; Syscall handler (int 0x80)
; eax = syscall number
; ebx = arg1, ecx = arg2, edx = arg3
; return value in eax
global syscall_handler
extern syscall_dispatch
syscall_handler:
    pusha
    
    ; Push args: edx, ecx, ebx, eax (order for C: eax, ebx, ecx, edx)
    push edx
    push ecx
    push ebx
    push eax
    call syscall_dispatch
    add esp, 16
    
    ; Store return value: overwrite eax slot in pusha frame
    ; pusha frame (from ESP): edi,esi,ebp,esp_dummy,ebx,edx,ecx,eax
    mov [esp + 28], eax
    
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
