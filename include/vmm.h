#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* Page table entry flags */
#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITETHROUGH (1ULL << 3)
#define PTE_NOCACHE     (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7)   /* 2 MB page (in PD entry) */
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)  /* No-Execute (if supported) */

/* Address mask for 4KB-aligned physical addresses in page table entries */
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

/* Page table index extraction macros (each level uses 9 bits) */
#define PML4_INDEX(addr)  (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x1FF)

void vmm_init(void);

/* True once EFER.NXE has actually been turned on (requires CPU support,
 * checked in vmm_init()). Callers must gate PTE_NX on this - setting that
 * bit while EFER.NXE=0 makes it a reserved bit instead of "ignored", and
 * every access through that page table entry will fault. */
int vmm_nx_supported(void);

/* Map a 4KB virtual page to a physical page */
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_map_page_in(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);

/* Unmap a 4KB virtual page. Returns the physical address that was mapped. */
uint64_t vmm_unmap_page(uint64_t virt);
uint64_t vmm_unmap_page_in(uint64_t cr3, uint64_t virt);

/* Walk page tables to get the physical address for a virtual address.
 * Returns 0 if not mapped. */
uint64_t vmm_get_physical(uint64_t virt);
uint64_t vmm_get_physical_in(uint64_t cr3, uint64_t virt);

/* User-pointer validation. Both assume `cr3` is the address space currently
 * loaded in CR3 (true for syscalls, which run on the calling process's page
 * tables without a context switch) - they check mapping/permission bits via
 * the page tables but read string bytes straight through the virtual
 * pointer, which only resolves correctly against the live CR3.
 * Reject (don't dereference) anything that doesn't pass these. */
int vmm_user_range_ok(uint64_t cr3, uint64_t virt, uint64_t size);
long vmm_user_strlen(uint64_t cr3, uint64_t virt, uint64_t max_len);

/* Invalidate a single TLB entry */
void vmm_invlpg(uint64_t addr);

/* Get current PML4 physical address (from CR3) */
uint64_t vmm_get_cr3(void);
void vmm_set_cr3(uint64_t cr3);

/* Create a process address space seeded with kernel mappings.
 * User mappings should be added with vmm_map_page_in(). */
uint64_t vmm_create_address_space(void);
void vmm_destroy_address_space(uint64_t cr3);
uint64_t vmm_get_kernel_cr3(void);

#endif
