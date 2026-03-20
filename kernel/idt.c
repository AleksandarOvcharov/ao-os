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

extern void irq0_handler(void);
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
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    memset(&idt, 0, sizeof(idt_entry_t) * IDT_ENTRIES);

    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint64_t)(uintptr_t)&idt;

    pic_remap();

    idt_set_gate(32,   (uint64_t)(uintptr_t)irq0_handler,   0x08, 0x8E); // 0x8E = present, DPL=0, 64-bit interrupt gate
    idt_set_gate(0x80, (uint64_t)(uintptr_t)syscall_handler, 0x08, 0xEE); // 0xEE = present, DPL=3, 64-bit interrupt gate

    idt_load(&idt_ptr);
}
