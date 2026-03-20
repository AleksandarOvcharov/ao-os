#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdint.h>

/* Register frame pushed by isr_common_stub in interrupt.asm.
 * Must match the push order exactly. */
typedef struct {
    /* Pushed by isr_common_stub */
    uint64_t cr2;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by ISR macro */
    uint64_t int_no, err_code;
    /* Pushed by CPU on interrupt */
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

void exception_handler(registers_t* regs);

#endif
