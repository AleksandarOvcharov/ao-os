#include "exception.h"
#include "vga.h"
#include "string.h"
#include "serial.h"

static const char* exception_names[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available (No FPU)",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception"
};

static void print_hex(uint64_t val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 17; i >= 2; i--) {
        int nibble = val & 0xF;
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        val >>= 4;
    }
    buf[18] = '\0';
    terminal_writestring(buf);
}

static void serial_print_hex(uint64_t val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 17; i >= 2; i--) {
        int nibble = val & 0xF;
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        val >>= 4;
    }
    buf[18] = '\0';
    serial_writestring(buf);
}

static void dump_register(const char* name, uint64_t val) {
    terminal_writestring("  ");
    terminal_writestring(name);
    terminal_writestring(" = ");
    print_hex(val);
}

void exception_handler(registers_t* regs) {
    asm volatile("cli");

    uint8_t panic_bg = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    uint8_t panic_title = vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLUE);
    uint8_t panic_highlight = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);
    uint8_t panic_reg = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLUE);

    /* Fill screen blue */
    terminal_setcolor(panic_bg);
    terminal_clear();
    for (int i = 0; i < 80 * 25; i++)
        terminal_putchar(' ');
    terminal_set_cursor(0, 0);

    /* Header */
    terminal_setcolor(panic_title);
    terminal_writestring("\n                       *** CPU EXCEPTION ***\n\n");

    /* Exception name */
    terminal_setcolor(panic_bg);
    terminal_writestring("  ");
    if (regs->int_no < 22) {
        terminal_writestring(exception_names[regs->int_no]);
    } else {
        terminal_writestring("Unknown Exception");
    }
    terminal_writestring(" (INT ");
    char num[4];
    int n = regs->int_no;
    if (n >= 10) { num[0] = '0' + n / 10; num[1] = '0' + n % 10; num[2] = ')'; num[3] = '\0'; }
    else { num[0] = '0' + n; num[1] = ')'; num[2] = '\0'; }
    terminal_writestring(num);
    terminal_writestring("\n");

    /* Error code */
    if (regs->err_code != 0) {
        terminal_setcolor(panic_highlight);
        terminal_writestring("  Error Code: ");
        terminal_setcolor(panic_bg);
        print_hex(regs->err_code);
        terminal_writestring("\n");
    }

    /* Page fault specific info */
    if (regs->int_no == 14) {
        terminal_setcolor(panic_highlight);
        terminal_writestring("  Faulting Address (CR2): ");
        terminal_setcolor(panic_bg);
        print_hex(regs->cr2);
        terminal_writestring("\n");
        terminal_writestring("  Cause: ");
        if (!(regs->err_code & 1)) terminal_writestring("page not present");
        else terminal_writestring("protection violation");
        if (regs->err_code & 2) terminal_writestring(", write");
        else terminal_writestring(", read");
        if (regs->err_code & 4) terminal_writestring(", user mode");
        else terminal_writestring(", kernel mode");
        terminal_writestring("\n");
    }

    /* Register dump */
    terminal_writestring("\n");
    terminal_setcolor(panic_highlight);
    terminal_writestring("  Register Dump:\n");
    terminal_setcolor(panic_reg);

    dump_register("RAX", regs->rax);
    dump_register("RBX", regs->rbx);
    terminal_writestring("\n");
    dump_register("RCX", regs->rcx);
    dump_register("RDX", regs->rdx);
    terminal_writestring("\n");
    dump_register("RSI", regs->rsi);
    dump_register("RDI", regs->rdi);
    terminal_writestring("\n");
    dump_register("RBP", regs->rbp);
    dump_register("R8 ", regs->r8);
    terminal_writestring("\n");
    dump_register("R9 ", regs->r9);
    dump_register("R10", regs->r10);
    terminal_writestring("\n");
    dump_register("R11", regs->r11);
    dump_register("R12", regs->r12);
    terminal_writestring("\n");
    dump_register("R13", regs->r13);
    dump_register("R14", regs->r14);
    terminal_writestring("\n");
    dump_register("R15", regs->r15);
    dump_register("CR2", regs->cr2);
    terminal_writestring("\n\n");

    /* Instruction pointer and flags from iretq frame */
    terminal_setcolor(panic_highlight);
    terminal_writestring("  Fault Location:\n");
    terminal_setcolor(panic_reg);
    dump_register("RIP", regs->rip);
    dump_register("CS ", regs->cs);
    terminal_writestring("\n");
    dump_register("RSP", regs->rsp);
    dump_register("SS ", regs->ss);
    terminal_writestring("\n");
    dump_register("FLG", regs->rflags);
    terminal_writestring("\n");

    /* Footer */
    terminal_setcolor(panic_bg);
    terminal_writestring("\n  System halted. Please reboot your computer.\n");

    /* Also dump to serial for debugging */
    serial_writestring("\n!!! CPU EXCEPTION: ");
    if (regs->int_no < 22)
        serial_writestring(exception_names[regs->int_no]);
    serial_writestring(" !!!\n");
    serial_writestring("  RIP="); serial_print_hex(regs->rip);
    serial_writestring("  ERR="); serial_print_hex(regs->err_code);
    serial_writestring("  CR2="); serial_print_hex(regs->cr2);
    serial_writestring("\n");

    /* Halt forever */
    while (1) {
        asm volatile("hlt");
    }
}
