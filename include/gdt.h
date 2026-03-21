#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

/* Update RSP0 in TSS (kernel stack for ring 3 -> ring 0 transitions) */
void tss_set_rsp0(uint64_t rsp0);

/* Assembly functions */
extern void gdt_load(void* gdt_descriptor);

/* Assembly-defined symbols */
extern char gdt_start[];
extern char gdt_end[];
extern char gdt_descriptor[];
extern char gdt_tss_entry[];
extern char tss[];

#endif
