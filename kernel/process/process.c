#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "string.h"
#include "klog.h"
#include "timer.h"
#include "syscall.h"

static process_t processes[MAX_PROCESSES];
static process_t* current_process = 0;
static uint64_t next_pid = 1;
static int scheduling_enabled = 0;
static uint64_t foreground_pid = 0;  /* see process_set_foreground() in process.h */

void process_set_foreground(uint64_t pid) {
    foreground_pid = pid;
}

int process_is_foreground(uint64_t pid) {
    return foreground_pid == pid;
}

/* Kernel stacks for processes 1-7 (PID 0 uses boot stack) */
static uint8_t kernel_stacks[MAX_PROCESSES][PROCESS_KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));

/* Scheduling */
#define TIME_QUANTUM 5  /* 50ms at 100Hz */
static uint32_t ticks_remaining = TIME_QUANTUM;

extern char stack_top[];  /* From boot.asm */

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_UNUSED;
        processes[i].pid = 0;
    }

    /* Create kernel process (PID 0) - represents current execution */
    process_t* p = &processes[0];
    p->pid = 0;
    p->state = PROC_RUNNING;
    strcpy(p->name, "kernel");
    p->is_user = 0;
    p->kernel_stack = 0;
    p->kernel_stack_top = (uint64_t)(uintptr_t)stack_top;
    p->kernel_rsp = 0;
    p->cr3 = vmm_get_cr3();
    p->exit_code = 0;
    p->detached = 0;
    p->wait_target = 0;
    p->wake_tick = 0;
    p->user_page_count = 0;

    current_process = p;

    klog_info("Process subsystem initialized");
}

void scheduler_start(void) {
    scheduling_enabled = 1;
    klog_info("Scheduler started");
}

process_t* process_get_current(void) {
    return current_process;
}

process_t* process_get(uint64_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_UNUSED && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return 0;
}

int process_is_running(uint64_t pid) {
    process_t* p = process_get(pid);
    return p && (p->state == PROC_RUNNING || p->state == PROC_READY);
}

int process_list(process_t** list) {
    *list = processes;
    return MAX_PROCESSES;
}

static void process_cleanup(process_t* p) {
    if (!p || p->pid == 0) return;

    syscall_close_process_fds(p->pid);

    for (int i = 0; i < p->user_page_count; i++) {
        if (p->user_pages[i]) pmm_free_page(p->user_pages[i]);
        p->user_pages[i] = 0;
    }
    p->user_page_count = 0;

    if (p->cr3 && p->cr3 != vmm_get_kernel_cr3()) vmm_destroy_address_space(p->cr3);
    p->cr3 = 0;
}

int process_map_user_page(process_t* p, uint64_t virt, uint64_t flags) {
    if (!p || !p->is_user || !p->cr3) return -1;

    /* Idempotency check: skip only if this page is already a *real* user
     * mapping. Every process's page tables start as a copy of the kernel's
     * own (vmm_create_address_space), which identity-maps the low address
     * range used by USER_CODE_BASE/USER_STACK_BASE/USER_API_BASE/
     * USER_ARGV_BASE as present+writable but NOT PTE_USER. Checking mere
     * presence here (as before) treated that inherited kernel mapping as
     * "already set up" and returned success without ever calling
     * vmm_map_page_in() - so the page kept its kernel-only permissions and
     * every ring 3 process page-faulted on first access to its own code,
     * stack, or args. Checking user-accessibility instead of presence
     * ensures we only skip pages *this* function already mapped. */
    if (vmm_user_range_ok(p->cr3, virt & ~(PAGE_SIZE - 1), 1)) return 0;
    if (p->user_page_count >= PROCESS_MAX_USER_PAGES) return -1;

    uint64_t phys = pmm_alloc_page();
    if (!phys) return -1;
    memset((void*)(uintptr_t)phys, 0, PAGE_SIZE);

    if (vmm_map_page_in(p->cr3, virt, phys, flags | PTE_USER) != 0) {
        pmm_free_page(phys);
        return -1;
    }

    p->user_pages[p->user_page_count++] = phys;
    return 0;
}

int process_write_user(process_t* p, uint64_t virt, const void* data, uint64_t size) {
    if (!p || !p->cr3 || (!data && size)) return -1;
    const uint8_t* src = (const uint8_t*)data;

    while (size > 0) {
        uint64_t phys = vmm_get_physical_in(p->cr3, virt);
        if (!phys) return -1;

        uint64_t page_left = PAGE_SIZE - (virt & (PAGE_SIZE - 1));
        uint64_t n = size < page_left ? size : page_left;
        memcpy((void*)(uintptr_t)phys, src, (size_t)n);
        virt += n;
        src += n;
        size -= n;
    }
    return 0;
}

int process_zero_user(process_t* p, uint64_t virt, uint64_t size) {
    if (!p || !p->cr3) return -1;

    while (size > 0) {
        uint64_t phys = vmm_get_physical_in(p->cr3, virt);
        if (!phys) return -1;

        uint64_t page_left = PAGE_SIZE - (virt & (PAGE_SIZE - 1));
        uint64_t n = size < page_left ? size : page_left;
        memset((void*)(uintptr_t)phys, 0, (size_t)n);
        virt += n;
        size -= n;
    }
    return 0;
}

int process_setup_user_args(process_t* p, const char* full_cmd) {
    if (!p || !full_cmd) return -1;

    uint8_t page[PAGE_SIZE];
    memset(page, 0, sizeof(page));

    uint64_t* base = (uint64_t*)page;
    char* strbuf = (char*)(page + 8 + 16 * 8);
    int strbuf_pos = 0;
    int argc = 0;
    const char* s = full_cmd;

    while (*s && argc < 16 && strbuf_pos < (int)(PAGE_SIZE - 8 - 16 * 8 - 1)) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char* tok = s;
        while (*s && *s != ' ') s++;
        int len = (int)(s - tok);
        if (strbuf_pos + len + 1 >= (int)(PAGE_SIZE - 8 - 16 * 8)) break;

        base[1 + argc] = USER_ARGV_BASE + 8 + 16 * 8 + (uint64_t)strbuf_pos;
        memcpy(strbuf + strbuf_pos, tok, (size_t)len);
        strbuf_pos += len;
        strbuf[strbuf_pos++] = '\0';
        argc++;
    }
    base[0] = (uint64_t)argc;
    return process_write_user(p, USER_ARGV_BASE, page, PAGE_SIZE);
}

process_t* process_create(const char* name, uint64_t entry_point, int is_user) {
    /* Find unused slot */
    process_t* p = 0;
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) {
            p = &processes[i];
            slot = i;
            break;
        }
    }
    if (!p) {
        klog_error("No free process slots");
        return 0;
    }

    p->pid = next_pid++;
    strncpy(p->name, name, 31);
    p->name[31] = '\0';
    p->is_user = is_user;
    p->cr3 = is_user ? vmm_create_address_space() : vmm_get_cr3();
    if (!p->cr3) {
        klog_error("Failed to create process address space");
        return 0;
    }
    p->exit_code = 0;
    p->detached = 0;
    p->wait_target = 0;
    p->wake_tick = 0;
    p->user_page_count = 0;

    /* Set up kernel stack */
    p->kernel_stack = kernel_stacks[slot];
    p->kernel_stack_top = (uint64_t)(uintptr_t)(p->kernel_stack + PROCESS_KERNEL_STACK_SIZE);
    memset(p->kernel_stack, 0, PROCESS_KERNEL_STACK_SIZE);

    /* Set up initial CPU context on kernel stack */
    cpu_context_t* ctx = (cpu_context_t*)(p->kernel_stack_top - sizeof(cpu_context_t));
    memset(ctx, 0, sizeof(cpu_context_t));

    ctx->rip = entry_point;
    ctx->rflags = 0x202;  /* IF=1, reserved bit 1 */

    if (is_user) {
        ctx->cs = SEL_USER_CS;
        ctx->ss = SEL_USER_DS;
        ctx->rsp = USER_STACK_TOP;

        /* Stack, API, and argv pages hold data, never code - mark them
         * non-executable (W^X) so a stack/data buffer overflow can't be
         * turned into shellcode execution. */
        uint64_t data_flags = PTE_WRITABLE | (vmm_nx_supported() ? PTE_NX : 0);

        /* User stack pages */
        for (int i = 0; i < PROCESS_USER_STACK_PAGES; i++) {
            uint64_t addr = USER_STACK_BASE + (uint64_t)i * PAGE_SIZE;
            if (process_map_user_page(p, addr, data_flags) != 0) {
                process_cleanup(p);
                p->state = PROC_UNUSED;
                return 0;
            }
        }

        /* API and argv pages */
        if (process_map_user_page(p, USER_API_BASE, data_flags) != 0 ||
            process_map_user_page(p, USER_ARGV_BASE, data_flags) != 0) {
            process_cleanup(p);
            p->state = PROC_UNUSED;
            return 0;
        }
    } else {
        ctx->cs = SEL_KERNEL_CS;
        ctx->ss = SEL_KERNEL_DS;
        ctx->rsp = p->kernel_stack_top;
    }

    p->kernel_rsp = (uint64_t)(uintptr_t)ctx;
    /* User processes aren't schedulable yet - the caller (aob_spawn/
     * elf_spawn) still has to map and write the code/API/argv pages after
     * this returns. Marking PROC_READY here would let the preemptible
     * scheduler switch to this process (entering ring 3 at its entry
     * point) before any of that setup has run, faulting on an
     * unmapped/not-yet-user-accessible code page. process_activate()
     * flips it to PROC_READY once setup actually finishes. Kernel
     * processes do no such setup, so they're ready immediately. */
    p->state = is_user ? PROC_LOADING : PROC_READY;

    klog_info("Process created");
    return p;
}

void process_activate(uint64_t pid) {
    process_t* p = process_get(pid);
    if (p && p->state == PROC_LOADING) p->state = PROC_READY;
}

void process_exit(int exit_code) {
    if (!current_process || current_process->pid == 0) return;

    /* schedule_tick() runs with interrupts already disabled (interrupt
     * gates, no sti before it) and switches away from any process whose
     * state is TERMINATED/BLOCKED/SLEEPING the instant it sees it. Setting
     * state = TERMINATED and then unblocking waiters are two separate
     * writes; if a timer tick landed between them, the scheduler would
     * switch this (now-dead) process out before the unblock loop ran, and
     * since a TERMINATED process never runs again, those waiters would
     * stay PROC_BLOCKED forever. cli/sti makes the pair atomic so the
     * timer can't observe the state in between. */
    asm volatile("cli");
    current_process->state = PROC_TERMINATED;
    current_process->exit_code = exit_code;

    /* Unblock any waiting process */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_BLOCKED &&
            (processes[i].wait_target == 0 || processes[i].wait_target == current_process->pid)) {
            processes[i].state = PROC_READY;
            processes[i].wait_target = 0;
        }
    }
    asm volatile("sti");

    /* Spin until scheduler switches us out */
    while (1) {
        asm volatile("sti; hlt");
    }
}

void process_wait(uint64_t pid) {
    process_waitpid(pid, 0, 0);
}

int process_waitpid(uint64_t pid, int* status, int options) {
    process_t* target = process_get(pid);
    if (!target || target->state == PROC_UNUSED || pid == 0) return -1;

    if (options & 1) {
        if (target->state != PROC_TERMINATED) return 0;
    } else {
        current_process->wait_target = pid;
        current_process->state = PROC_BLOCKED;

        while (target->state != PROC_TERMINATED && target->state != PROC_UNUSED) {
            asm volatile("sti; hlt");
        }

        current_process->state = PROC_RUNNING;
        current_process->wait_target = 0;
    }

    if (target->state == PROC_UNUSED) return -1;
    if (status) *status = target->exit_code;
    int exit_code = target->exit_code;

    /* Clean up terminated process */
    process_cleanup(target);
    target->state = PROC_UNUSED;
    target->pid = 0;
    return exit_code;
}

void process_detach(uint64_t pid) {
    process_t* p = process_get(pid);
    if (p && p->pid != 0) p->detached = 1;
}

int process_reap_terminated(void) {
    int reaped = 0;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_TERMINATED && processes[i].detached) {
            process_cleanup(&processes[i]);
            processes[i].state = PROC_UNUSED;
            processes[i].pid = 0;
            processes[i].detached = 0;
            reaped++;
        }
    }
    return reaped;
}

static void wake_sleepers(void) {
    uint64_t now = timer_get_ticks();
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_SLEEPING && processes[i].wake_tick <= now) {
            processes[i].wake_tick = 0;
            processes[i].state = PROC_READY;
        }
    }
}

void process_sleep_ticks(uint64_t ticks) {
    if (!current_process || current_process->pid == 0 || ticks == 0) return;
    current_process->wake_tick = timer_get_ticks() + ticks;
    current_process->state = PROC_SLEEPING;
    while (current_process->state == PROC_SLEEPING) {
        asm volatile("sti; hlt");
    }
}

void process_terminate(uint64_t pid) {
    process_t* p = process_get(pid);
    if (!p || p->pid == 0) return;

    /* Same hazard as process_exit(): state must become TERMINATED and the
     * blocked-process sweep must happen as one atomic step, or a timer
     * tick landing in between can leave waiters stuck forever. */
    asm volatile("cli");
    p->state = PROC_TERMINATED;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_BLOCKED) {
            processes[i].state = PROC_READY;
        }
    }
    asm volatile("sti");
}

/* Called from timer IRQ handler (interrupt.asm).
 * Returns the kernel stack pointer to restore (may be a different process). */
uint64_t schedule_tick(uint64_t current_rsp) {
    timer_tick();
    wake_sleepers();

    if (!scheduling_enabled || !current_process) return current_rsp;

    /* Check if we need to switch */
    int need_switch = 0;

    if (current_process->state == PROC_TERMINATED ||
        current_process->state == PROC_BLOCKED ||
        current_process->state == PROC_SLEEPING) {
        need_switch = 1;
    } else {
        if (ticks_remaining > 0) ticks_remaining--;
        if (ticks_remaining == 0) {
            ticks_remaining = TIME_QUANTUM;
            need_switch = 1;
        }
    }

    if (!need_switch) return current_rsp;

    /* Save current process state */
    current_process->kernel_rsp = current_rsp;
    if (current_process->state == PROC_RUNNING) {
        current_process->state = PROC_READY;
    }

    /* Find next ready process (round-robin) */
    process_t* next = 0;
    int start = (int)(current_process - processes);
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (start + i) % MAX_PROCESSES;
        if (processes[idx].state == PROC_READY) {
            next = &processes[idx];
            break;
        }
    }

    if (!next) {
        /* No ready process, continue with current if possible */
        if (current_process->state == PROC_READY) {
            current_process->state = PROC_RUNNING;
        }
        return current_rsp;
    }

    /* Context switch */
    next->state = PROC_RUNNING;
    current_process = next;

    /* Update TSS RSP0 for ring 3 -> ring 0 transitions */
    tss_set_rsp0(next->kernel_stack_top);
    if ((vmm_get_cr3() & PTE_ADDR_MASK) != (next->cr3 & PTE_ADDR_MASK)) {
        vmm_set_cr3(next->cr3);
    }

    ticks_remaining = TIME_QUANTUM;

    return next->kernel_rsp;
}
