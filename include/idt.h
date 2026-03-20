#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// 64-bit IDT entry (16 bytes)
typedef struct {
    uint16_t base_low;    // Handler address bits 0-15
    uint16_t selector;    // Code segment selector
    uint8_t  ist;         // IST index (bits 0-2), rest zero
    uint8_t  flags;       // Type(4) + Zero(1) + DPL(2) + Present(1)
    uint16_t base_mid;    // Handler address bits 16-31
    uint32_t base_high;   // Handler address bits 32-63
    uint32_t reserved;    // Must be zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

extern void idt_load(void*);
extern void irq0_handler(void);

#endif
