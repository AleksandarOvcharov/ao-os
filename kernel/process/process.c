#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "string.h"
#include "klog.h"
#include "timer.h"

static process_t processes[MAX_PROCESSES];
static process_t* current_process = 0;
static uint64_t next_pid = 1;
static int scheduling_enabled = 0;

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

    current_process = p;

    /* Reserve memory regions for user programs */
    pmm_mark_region_used(USER_CODE_BASE, 16384);
    pmm_mark_region_used(USER_STACK_BASE, USER_STACK_SIZE);

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
    p->cr3 = vmm_get_cr3();
    p->exit_code = 0;

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

        /* Map user-accessible pages */
        /* Code pages (0x200000, 4 pages = 16KB) */
        for (int i = 0; i < 4; i++) {
            uint64_t addr = USER_CODE_BASE + (uint64_t)i * PAGE_SIZE;
            vmm_map_page(addr, addr, PTE_WRITABLE | PTE_USER);
        }

        /* User stack pages (0x800000, 4 pages = 16KB) */
        for (int i = 0; i < PROCESS_USER_STACK_PAGES; i++) {
            uint64_t addr = USER_STACK_BASE + (uint64_t)i * PAGE_SIZE;
            vmm_map_page(addr, addr, PTE_WRITABLE | PTE_USER);
        }

        /* API and argv pages */
        vmm_map_page(USER_API_BASE, USER_API_BASE, PTE_WRITABLE | PTE_USER);
        vmm_map_page(USER_ARGV_BASE, USER_ARGV_BASE, PTE_WRITABLE | PTE_USER);
    } else {
        ctx->cs = SEL_KERNEL_CS;
        ctx->ss = SEL_KERNEL_DS;
        ctx->rsp = p->kernel_stack_top;
    }

    p->kernel_rsp = (uint64_t)(uintptr_t)ctx;
    p->state = PROC_READY;

    klog_info("Process created");
    return p;
}

void process_exit(int exit_code) {
    if (!current_process || current_process->pid == 0) return;

    current_process->state = PROC_TERMINATED;
    current_process->exit_code = exit_code;

    /* Unblock any waiting process */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_BLOCKED) {
            processes[i].state = PROC_READY;
        }
    }

    /* Spin until scheduler switches us out */
    while (1) {
        asm volatile("sti; hlt");
    }
}

void process_wait(uint64_t pid) {
    process_t* target = process_get(pid);
    if (!target || target->state == PROC_UNUSED) return;

    current_process->state = PROC_BLOCKED;

    while (target->state != PROC_TERMINATED && target->state != PROC_UNUSED) {
        asm volatile("sti; hlt");
    }

    current_process->state = PROC_RUNNING;

    /* Clean up terminated process */
    target->state = PROC_UNUSED;
}

void process_terminate(uint64_t pid) {
    process_t* p = process_get(pid);
    if (!p || p->pid == 0) return;

    p->state = PROC_TERMINATED;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_BLOCKED) {
            processes[i].state = PROC_READY;
        }
    }
}

/* Called from timer IRQ handler (interrupt.asm).
 * Returns the kernel stack pointer to restore (may be a different process). */
uint64_t schedule_tick(uint64_t current_rsp) {
    timer_tick();

    if (!scheduling_enabled || !current_process) return current_rsp;

    /* Check if we need to switch */
    int need_switch = 0;

    if (current_process->state == PROC_TERMINATED ||
        current_process->state == PROC_BLOCKED) {
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

    ticks_remaining = TIME_QUANTUM;

    return next->kernel_rsp;
}
