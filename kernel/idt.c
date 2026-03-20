#include "idt.h"
#include "io.h"
#include "string.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = (uint16_t)(base & 0xFFFF);
    idt[num].base_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].base_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].selector  = sel;
    idt[num].ist       = 0;
    idt[num].flags     = flags;
    idt[num].reserved  = 0;
}

/* Set an IDT gate with a specific IST entry */
void idt_set_gate_ist(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt_set_gate(num, base, sel, flags);
    idt[num].ist = ist & 0x07;
}

/* ISR stubs from interrupt.asm */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);

/* IRQ handlers from interrupt.asm */
extern void irq0_handler(void);
extern void irq1_handler(void);
extern void syscall_handler(void);

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    /* Enable IRQ0 (timer) and IRQ1 (keyboard), mask others */
    outb(0x21, 0xFC);  /* 11111100 = IRQ0+IRQ1 enabled */
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    memset(&idt, 0, sizeof(idt_entry_t) * IDT_ENTRIES);

    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint64_t)(uintptr_t)&idt;

    pic_remap();

    /* CPU Exception Handlers (ISR 0-21) */
    idt_set_gate(0,  (uint64_t)(uintptr_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint64_t)(uintptr_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint64_t)(uintptr_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint64_t)(uintptr_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint64_t)(uintptr_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint64_t)(uintptr_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint64_t)(uintptr_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint64_t)(uintptr_t)isr7,  0x08, 0x8E);
    /* Double fault uses IST1 for a dedicated stack */
    idt_set_gate_ist(8,  (uint64_t)(uintptr_t)isr8,  0x08, 0x8E, 1);
    idt_set_gate(9,  (uint64_t)(uintptr_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint64_t)(uintptr_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)(uintptr_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)(uintptr_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)(uintptr_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)(uintptr_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)(uintptr_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)(uintptr_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)(uintptr_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)(uintptr_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)(uintptr_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)(uintptr_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)(uintptr_t)isr21, 0x08, 0x8E);

    /* Hardware IRQ Handlers (remapped to INT 32+) */
    idt_set_gate(32,   (uint64_t)(uintptr_t)irq0_handler,   0x08, 0x8E); /* Timer */
    idt_set_gate(33,   (uint64_t)(uintptr_t)irq1_handler,   0x08, 0x8E); /* Keyboard */

    /* Syscall (int 0x80) - DPL=3 so user programs can invoke it */
    idt_set_gate(0x80, (uint64_t)(uintptr_t)syscall_handler, 0x08, 0xEE);

    idt_load(&idt_ptr);
}
