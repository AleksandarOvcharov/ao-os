#include "gdt.h"
#include "klog.h"

/* IST1 stack for double fault handler (4 KB) */
static uint8_t ist1_stack[4096] __attribute__((aligned(16)));

void gdt_init(void) {
    /* Fill in the TSS descriptor in the GDT at runtime,
     * since we need the actual address of the TSS. */
    uint64_t tss_addr = (uint64_t)(uintptr_t)tss;
    uint16_t tss_limit = 103; /* sizeof(TSS) - 1 */

    uint8_t* entry = (uint8_t*)gdt_tss_entry;
    /* Limit low */
    entry[0] = tss_limit & 0xFF;
    entry[1] = (tss_limit >> 8) & 0xFF;
    /* Base low */
    entry[2] = tss_addr & 0xFF;
    entry[3] = (tss_addr >> 8) & 0xFF;
    /* Base mid-low */
    entry[4] = (tss_addr >> 16) & 0xFF;
    /* Access byte: 0x89 = present, 64-bit TSS available */
    entry[5] = 0x89;
    /* Flags (high nibble) + Limit high (low nibble) */
    entry[6] = 0x00;
    /* Base mid-high */
    entry[7] = (tss_addr >> 24) & 0xFF;
    /* Base high (32 bits) */
    entry[8]  = (tss_addr >> 32) & 0xFF;
    entry[9]  = (tss_addr >> 40) & 0xFF;
    entry[10] = (tss_addr >> 48) & 0xFF;
    entry[11] = (tss_addr >> 56) & 0xFF;
    /* Reserved */
    entry[12] = 0;
    entry[13] = 0;
    entry[14] = 0;
    entry[15] = 0;

    /* Set up TSS fields */

    /* RSP0: kernel stack for ring 3 -> ring 0 transitions.
     * We use the same stack as the kernel for now. */
    extern char stack_top[];
    uint64_t rsp0 = (uint64_t)(uintptr_t)stack_top;
    /* RSP0 is at offset 4 in TSS (after 4-byte reserved field) */
    uint8_t* tss_bytes = (uint8_t*)(uintptr_t)tss;
    *(uint64_t*)(tss_bytes + 4) = rsp0;

    /* IST1: dedicated stack for double fault handler */
    uint64_t ist1_top = (uint64_t)(uintptr_t)&ist1_stack[sizeof(ist1_stack)];
    *(uint64_t*)(tss_bytes + 36) = ist1_top;

    /* Load the new GDT */
    gdt_load(gdt_descriptor);

    /* Load TSS (selector 0x18) */
    asm volatile("mov $0x18, %%ax; ltr %%ax" ::: "ax");

    klog_info("GDT and TSS initialized");
}
