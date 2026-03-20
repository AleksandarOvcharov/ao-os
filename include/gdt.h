#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

/* Assembly functions */
extern void gdt_load(void* gdt_descriptor);

/* Assembly-defined symbols */
extern char gdt_start[];
extern char gdt_end[];
extern char gdt_descriptor[];
extern char gdt_tss_entry[];
extern char tss[];

#endif
