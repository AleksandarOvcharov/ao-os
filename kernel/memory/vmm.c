#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "klog.h"
#include "cpu.h"

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
#define PTE_PRIVATE_TABLE (1ULL << 9)

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

void vmm_set_cr3(uint64_t cr3) {
    asm volatile("mov %0, %%cr3" :: "r"(cr3 & PTE_ADDR_MASK) : "memory");
}

/* Allocate a zeroed page for use as a page table */
static uint64_t alloc_table_page(void) {
    uint64_t page = pmm_alloc_page();
    if (page == 0) return 0;
    memset(phys_to_virt(page), 0, PAGE_SIZE);
    return page;
}

static int nx_available = 0;

#define IA32_EFER     0xC0000080
#define EFER_NXE      (1ULL << 11)

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    asm volatile("wrmsr" :: "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32)));
}

void vmm_init(void) {
    /* Use the bootloader's existing PML4 (identity-mapped 0-2GB with 2MB pages).
     * We manage it in-place rather than allocating a new one, which avoids
     * any risk of a bad CR3 switch during early boot. The VMM API (map/unmap)
     * works directly on these page tables. */
    kernel_pml4_phys = vmm_get_cr3() & PTE_ADDR_MASK;

    /* PTE_NX (bit 63) is reserved, not "ignored", when EFER.NXE=0 - setting
     * it on an entry used for translation would fault every access through
     * that entry. Only start handing it out once the CPU has NX and we've
     * actually turned EFER.NXE on. */
    if (cpu_get_info()->ext_features & CPU_EXT_NX) {
        wrmsr(IA32_EFER, rdmsr(IA32_EFER) | EFER_NXE);
        nx_available = 1;
        klog_info("NX (no-execute) page protection enabled");
    } else {
        klog_warn("CPU has no NX support - user pages cannot be marked non-executable");
    }

    klog_info("VMM initialized with 4-level paging");
}

int vmm_nx_supported(void) {
    return nx_available;
}

uint64_t vmm_get_kernel_cr3(void) {
    return kernel_pml4_phys;
}

uint64_t vmm_create_address_space(void) {
    uint64_t pml4_page = alloc_table_page();
    if (!pml4_page) return 0;

    /* Copy kernel mappings. Private process mappings are installed with
     * copy-on-map of intermediate page-table levels below. */
    memcpy(phys_to_virt(pml4_page), phys_to_virt(kernel_pml4_phys), PAGE_SIZE);
    return pml4_page;
}

static uint64_t copy_table_page(uint64_t src_phys) {
    uint64_t dst = pmm_alloc_page();
    if (!dst) return 0;
    memcpy(phys_to_virt(dst), phys_to_virt(src_phys), PAGE_SIZE);
    return dst;
}

static int ensure_private_entry(uint64_t cr3, uint64_t* table, uint64_t idx, uint64_t flags) {
    if ((cr3 & PTE_ADDR_MASK) == kernel_pml4_phys) return 0;
    if (!(table[idx] & PTE_PRESENT)) return 0;
    if (table[idx] & PTE_PRIVATE_TABLE) return 0;

    uint64_t child = table[idx] & PTE_ADDR_MASK;
    uint64_t copy = copy_table_page(child);
    if (!copy) return -1;
    table[idx] = copy | ((table[idx] | flags | PTE_PRIVATE_TABLE) & ~PTE_ADDR_MASK);
    return 0;
}

int vmm_map_page_in(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags) {
    /* Ensure addresses are page-aligned */
    virt &= PTE_ADDR_MASK;
    phys &= PTE_ADDR_MASK;
    flags &= ~PTE_ADDR_MASK;

    cr3 &= PTE_ADDR_MASK;
    if (!cr3) return -1;
    uint64_t* pml4 = phys_to_virt(cr3);

    /* Determine parent entry flags - propagate PTE_USER if requested */
    uint64_t parent_flags = PTE_PRESENT | PTE_WRITABLE;
    if (flags & PTE_USER) parent_flags |= PTE_USER;

    /* Level 4: PML4 → PDPT */
    uint64_t pml4_idx = PML4_INDEX(virt);
    if (ensure_private_entry(cr3, pml4, pml4_idx, parent_flags) != 0) return -1;
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t pdpt_page = alloc_table_page();
        if (pdpt_page == 0) return -1;
        pml4[pml4_idx] = pdpt_page | parent_flags | PTE_PRIVATE_TABLE;
    } else if (flags & PTE_USER) {
        pml4[pml4_idx] |= PTE_USER;
    }
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    /* Level 3: PDPT → PD */
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    if (ensure_private_entry(cr3, pdpt, pdpt_idx, parent_flags) != 0) return -1;
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t pd_page = alloc_table_page();
        if (pd_page == 0) return -1;
        pdpt[pdpt_idx] = pd_page | parent_flags | PTE_PRIVATE_TABLE;
    } else if (flags & PTE_USER) {
        pdpt[pdpt_idx] |= PTE_USER;
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
        pd[pd_idx] = pt_page | parent_flags | PTE_PRIVATE_TABLE;
    }

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t pt_page = alloc_table_page();
        if (pt_page == 0) return -1;
        pd[pd_idx] = pt_page | parent_flags | PTE_PRIVATE_TABLE;
    } else if (flags & PTE_USER) {
        pd[pd_idx] |= PTE_USER;
    }
    uint64_t* pt = phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);

    /* Level 1: PT → page */
    uint64_t pt_idx = PT_INDEX(virt);
    pt[pt_idx] = phys | flags | PTE_PRESENT;

    if (cr3 == (vmm_get_cr3() & PTE_ADDR_MASK)) vmm_invlpg(virt);
    return 0;
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    return vmm_map_page_in(kernel_pml4_phys, virt, phys, flags);
}

uint64_t vmm_unmap_page_in(uint64_t cr3, uint64_t virt) {
    virt &= PTE_ADDR_MASK;

    cr3 &= PTE_ADDR_MASK;
    if (!cr3) return 0;
    uint64_t* pml4 = phys_to_virt(cr3);

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

    if (cr3 == (vmm_get_cr3() & PTE_ADDR_MASK)) vmm_invlpg(virt);
    return old_phys;
}

uint64_t vmm_unmap_page(uint64_t virt) {
    return vmm_unmap_page_in(kernel_pml4_phys, virt);
}

uint64_t vmm_get_physical_in(uint64_t cr3, uint64_t virt) {
    cr3 &= PTE_ADDR_MASK;
    if (!cr3) return 0;
    uint64_t* pml4 = phys_to_virt(cr3);

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

uint64_t vmm_get_physical(uint64_t virt) {
    return vmm_get_physical_in(kernel_pml4_phys, virt);
}

/* Returns 1 if the single 4KB page containing `virt` is present and marked
 * PTE_USER at every level of the walk (i.e. actually reachable from ring 3),
 * 0 otherwise. Used to validate pointers a syscall receives from user code
 * before the kernel dereferences them - a page that is merely *present*
 * (e.g. a kernel mapping seeded into every process's address space) must
 * not be treated as user-accessible. */
static int vmm_user_page_ok(uint64_t cr3, uint64_t virt) {
    cr3 &= PTE_ADDR_MASK;
    if (!cr3) return 0;
    uint64_t* pml4 = phys_to_virt(cr3);

    uint64_t pml4_idx = PML4_INDEX(virt);
    if ((pml4[pml4_idx] & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)) return 0;
    uint64_t* pdpt = phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    uint64_t pdpt_idx = PDPT_INDEX(virt);
    if ((pdpt[pdpt_idx] & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)) return 0;
    uint64_t* pd = phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    uint64_t pd_idx = PD_INDEX(virt);
    if ((pd[pd_idx] & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)) return 0;

    if (pd[pd_idx] & PTE_HUGE) return 1;  /* 2MB page, already checked */

    uint64_t* pt = phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
    uint64_t pt_idx = PT_INDEX(virt);
    return (pt[pt_idx] & (PTE_PRESENT | PTE_USER)) == (PTE_PRESENT | PTE_USER);
}

/* Validates that every byte in [virt, virt+size) belongs to a page mapped
 * PTE_USER in the given address space. size == 0 is rejected: callers must
 * not use a zero-length range to skip validation. Returns 1 if the whole
 * range is user-accessible, 0 otherwise (including on virt+size overflow). */
int vmm_user_range_ok(uint64_t cr3, uint64_t virt, uint64_t size) {
    if (size == 0) return 0;
    if (virt + size < virt) return 0;  /* overflow */

    uint64_t page = virt & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end = virt + size;
    for (; page < end; page += PAGE_SIZE) {
        if (!vmm_user_page_ok(cr3, page)) return 0;
    }
    return 1;
}

/* Validates a NUL-terminated user string starting at `virt`, up to and
 * including `max_len` bytes of content (NUL not counted). Returns the
 * string length on success, or -1 if the NUL wasn't found within max_len
 * bytes or any touched byte lies outside a user-accessible page. */
long vmm_user_strlen(uint64_t cr3, uint64_t virt, uint64_t max_len) {
    uint64_t page = virt & ~(uint64_t)(PAGE_SIZE - 1);
    if (!vmm_user_page_ok(cr3, page)) return -1;

    for (uint64_t i = 0; i < max_len; i++) {
        uint64_t addr = virt + i;
        uint64_t this_page = addr & ~(uint64_t)(PAGE_SIZE - 1);
        if (this_page != page) {
            page = this_page;
            if (!vmm_user_page_ok(cr3, page)) return -1;
        }
        if (*(const char*)(uintptr_t)addr == '\0') return (long)i;
    }
    return -1;
}

static void free_private_tables(uint64_t table_phys, int level) {
    uint64_t* table = phys_to_virt(table_phys);
    for (int i = 0; i < 512; i++) {
        if (!(table[i] & PTE_PRESENT)) continue;
        if (!(table[i] & PTE_PRIVATE_TABLE)) continue;
        if (table[i] & PTE_HUGE) continue;

        uint64_t child = table[i] & PTE_ADDR_MASK;
        if (level > 1) {
            free_private_tables(child, level - 1);
        }
        pmm_free_page(child);
        table[i] = 0;
    }
}

void vmm_destroy_address_space(uint64_t cr3) {
    cr3 &= PTE_ADDR_MASK;
    if (!cr3 || cr3 == kernel_pml4_phys) return;
    free_private_tables(cr3, 3);
    pmm_free_page(cr3);
}
