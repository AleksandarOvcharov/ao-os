#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12

/* E820 memory map stored by bootloader at physical address 0x6000
 * (below boot sector at 0x7C00, safe from stage2 code overlap) */
#define E820_MAP_ADDR   0x6000

/* E820 memory region types */
#define E820_USABLE     1
#define E820_RESERVED   2
#define E820_ACPI_RECL  3
#define E820_ACPI_NVS   4
#define E820_BAD        5

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed)) e820_entry_t;

void pmm_init(void);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t addr);
void pmm_mark_region_used(uint64_t base, uint64_t length);
uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_total_memory(void);

/* Read E820 map entry count */
uint32_t pmm_get_e820_count(void);
e820_entry_t* pmm_get_e820_entries(void);

#endif
