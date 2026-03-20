#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "klog.h"

/* 4-Level Page Table Manager for x86-64.
 *
 * The bootloader sets up identity-mapped 2MB pages for 0-2GB.
 * We keep that mapping and provide functions to manage individual
 * 4KB pages. The existing 2MB pages remain for efficiency.
 *
 * Page table hierarchy (each table = 4KB, 512 entries × 8 bytes):
 *   PML4 → PDPT → PD → PT → 4KB page
 *                    └→ 2MB page (if PTE_HUGE set)
 */

static uint64_t kernel_pml4_phys = 0;

/* Since we're identity-mapped, phys addr == virt addr for kernel memory */
static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(uintptr_t)phys;
}

void vmm_invlpg(uint64_t addr) {
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

uint64_t vmm_get_cr3(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/* Allocate a zeroed page for use as a page table */
static uint64_t alloc_table_page(void) {
    uint64_t page = pmm_alloc_page();
    if (page == 0) return 0;
    memset(phys_to_virt(page), 0, PAGE_SIZE);
    return page;
}

void vmm_init(void) {
    /* Use the bootloader's existing PML4 (identity-mapped 0-2GB with 2MB pages).
     * We manage it in-place rather than allocating a new one, which avoids
     * any risk of a bad CR3 switch during early boot. The VMM API (map/unmap)
     * works directly on these page tables. */
    kernel_pml4_phys = vmm_get_cr3() & PTE_ADDR_MASK;

    klog_info("VMM initialized with 4-level paging");
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    /* Ensure addresses are page-aligned */
    virt &= PTE_ADDR_MASK;
    phys &= PTE_ADDR_MASK;
    flags &= ~PTE_ADDR_MASK;

    uint64_t* pml4 = phys_to_virt(kernel_pml4_phys);

    /* Level 4: PML4 → PDPT */
    uint64_t pml4_idx = PML4_INDEX(virt);
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t pdpt_page = alloc_table_page();
        if (pdpt_page == 0) return -1;
        pml4[pml4_idx] = pdpt_page | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    /* Level 3: PDPT → PD */
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t pd_page = alloc_table_page();
        if (pd_page == 0) return -1;
        pdpt[pdpt_idx] = pd_page | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    /* Level 2: PD → PT
     * If this PD entry is a 2MB huge page, we need to split it first */
    uint64_t pd_idx = PD_INDEX(virt);
    if ((pd[pd_idx] & PTE_PRESENT) && (pd[pd_idx] & PTE_HUGE)) {
        /* Split 2MB page into 512 × 4KB pages */
        uint64_t huge_base = pd[pd_idx] & PTE_ADDR_MASK;
        uint64_t huge_flags = pd[pd_idx] & ~PTE_ADDR_MASK & ~PTE_HUGE;

        uint64_t pt_page = alloc_table_page();
        if (pt_page == 0) return -1;
        uint64_t* pt = phys_to_virt(pt_page);

        /* Fill PT with 512 entries mapping the same physical range */
        for (int i = 0; i < 512; i++) {
            pt[i] = (huge_base + (uint64_t)i * PAGE_SIZE) | huge_flags;
        }

        /* Replace the 2MB PD entry with a pointer to the new PT */
        pd[pd_idx] = pt_page | PTE_PRESENT | PTE_WRITABLE;
    }

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t pt_page = alloc_table_page();
        if (pt_page == 0) return -1;
        pd[pd_idx] = pt_page | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);

    /* Level 1: PT → page */
    uint64_t pt_idx = PT_INDEX(virt);
    pt[pt_idx] = phys | flags | PTE_PRESENT;

    vmm_invlpg(virt);
    return 0;
}

uint64_t vmm_unmap_page(uint64_t virt) {
    virt &= PTE_ADDR_MASK;

    uint64_t* pml4 = phys_to_virt(kernel_pml4_phys);

    uint64_t pml4_idx = PML4_INDEX(virt);
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    uint64_t pdpt_idx = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    uint64_t pd_idx = PD_INDEX(virt);
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;

    /* If it's a 2MB huge page, can't unmap a single 4KB sub-page */
    if (pd[pd_idx] & PTE_HUGE) return 0;

    uint64_t* pt = phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
    uint64_t pt_idx = PT_INDEX(virt);

    uint64_t old_phys = pt[pt_idx] & PTE_ADDR_MASK;
    pt[pt_idx] = 0;

    vmm_invlpg(virt);
    return old_phys;
}

uint64_t vmm_get_physical(uint64_t virt) {
    uint64_t* pml4 = phys_to_virt(kernel_pml4_phys);

    uint64_t pml4_idx = PML4_INDEX(virt);
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    uint64_t pdpt_idx = PDPT_INDEX(virt);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    uint64_t pd_idx = PD_INDEX(virt);
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;

    /* 2MB huge page */
    if (pd[pd_idx] & PTE_HUGE) {
        uint64_t base = pd[pd_idx] & PTE_ADDR_MASK;
        uint64_t offset = virt & 0x1FFFFF;  /* offset within 2MB page */
        return base + offset;
    }

    uint64_t* pt = phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
    uint64_t pt_idx = PT_INDEX(virt);
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    uint64_t base = pt[pt_idx] & PTE_ADDR_MASK;
    uint64_t offset = virt & 0xFFF;  /* offset within 4KB page */
    return base + offset;
}
