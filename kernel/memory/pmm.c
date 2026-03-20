#include "pmm.h"
#include "string.h"
#include "klog.h"

/* Bitmap-based physical page frame allocator.
 * 1 bit per 4KB page. Bit=1 means used, bit=0 means free. */

#define MAX_MEMORY      (4ULL * 1024 * 1024 * 1024)  /* Support up to 4 GB */
#define MAX_PAGES       (MAX_MEMORY / PAGE_SIZE)       /* 1,048,576 pages */
#define BITMAP_SIZE     (MAX_PAGES / 8)                /* 128 KB bitmap */

/* Bitmap stored in BSS (zeroed at startup = all free) */
static uint8_t page_bitmap[BITMAP_SIZE];

static uint64_t total_pages = 0;
static uint64_t free_pages = 0;
static uint64_t total_memory_bytes = 0;

/* Linker symbols */
extern char _kernel_start[];
extern char _kernel_end[];

static inline void bitmap_set(uint64_t page) {
    page_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bitmap_test(uint64_t page) {
    return (page_bitmap[page / 8] >> (page % 8)) & 1;
}

/* Mark a range of pages as used */
static void mark_region_used(uint64_t base, uint64_t length) {
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t end_page = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (end_page > MAX_PAGES) end_page = MAX_PAGES;
    for (uint64_t p = start_page; p < end_page; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            if (free_pages > 0) free_pages--;
        }
    }
}

/* Mark a range of pages as free */
static void mark_region_free(uint64_t base, uint64_t length) {
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t end_page = (base + length) / PAGE_SIZE;
    if (end_page > MAX_PAGES) end_page = MAX_PAGES;
    for (uint64_t p = start_page; p < end_page; p++) {
        if (bitmap_test(p)) {
            bitmap_clear(p);
            free_pages++;
        }
    }
}

uint32_t pmm_get_e820_count(void) {
    /* Bootloader stores count as 16-bit word at E820_MAP_ADDR */
    return *(volatile uint16_t*)E820_MAP_ADDR;
}

e820_entry_t* pmm_get_e820_entries(void) {
    return (e820_entry_t*)(E820_MAP_ADDR + 4);
}

void pmm_init(void) {
    uint32_t e820_count = pmm_get_e820_count();
    e820_entry_t* entries = pmm_get_e820_entries();

    /* Start with all pages marked used */
    memset(page_bitmap, 0xFF, BITMAP_SIZE);
    free_pages = 0;
    total_pages = 0;
    total_memory_bytes = 0;

    /* If no E820 data, assume 32 MB usable starting at 1 MB */
    if (e820_count == 0 || e820_count > 128) {
        klog_warn("No E820 memory map found, assuming 32 MB RAM");
        total_memory_bytes = 32ULL * 1024 * 1024;
        total_pages = total_memory_bytes / PAGE_SIZE;
        /* Mark 1 MB to 32 MB as free */
        mark_region_free(0x100000, 31ULL * 1024 * 1024);
    } else {
        /* Process E820 entries */
        for (uint32_t i = 0; i < e820_count; i++) {
            uint64_t base = entries[i].base;
            uint64_t length = entries[i].length;
            uint64_t end = base + length;

            if (end > total_memory_bytes)
                total_memory_bytes = end;

            if (entries[i].type == E820_USABLE) {
                /* Only consider memory below our MAX_MEMORY */
                if (base < MAX_MEMORY) {
                    if (end > MAX_MEMORY) length = MAX_MEMORY - base;
                    mark_region_free(base, length);
                }
            }
        }
        total_pages = total_memory_bytes / PAGE_SIZE;
        if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;
    }

    /* Reserve low memory (0 - 1 MB): BIOS, IVT, VGA, bootloader page tables, E820 map */
    mark_region_used(0, 0x100000);

    /* Reserve kernel code + data + BSS */
    uint64_t kstart = (uint64_t)(uintptr_t)_kernel_start;
    uint64_t kend = (uint64_t)(uintptr_t)_kernel_end;
    mark_region_used(kstart, kend - kstart);

    /* Reserve the page_bitmap itself (it's in kernel BSS, already covered above) */

    klog_info("PMM initialized");
}

void pmm_mark_region_used(uint64_t base, uint64_t length) {
    mark_region_used(base, length);
}

uint64_t pmm_alloc_page(void) {
    /* Search bitmap for a free page, starting above 1 MB */
    uint64_t start = 0x100000 / PAGE_SIZE;  /* Skip low memory */
    for (uint64_t i = start; i < MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return i * PAGE_SIZE;
        }
    }
    return 0; /* Out of memory */
}

void pmm_free_page(uint64_t addr) {
    uint64_t page = addr / PAGE_SIZE;
    if (page >= MAX_PAGES) return;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

uint64_t pmm_get_total_pages(void) {
    return total_pages;
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}

uint64_t pmm_get_total_memory(void) {
    return total_memory_bytes;
}
