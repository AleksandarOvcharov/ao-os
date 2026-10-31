#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/* Each extra slot costs one process_t (modest) plus one
 * PROCESS_KERNEL_STACK_SIZE kernel stack (8 KiB) of static BSS - 16 slots
 * costs ~128 KiB total, trivial against the 32+ MiB this kernel targets.
 * This doesn't remove the fixed ceiling (process_create() still fails
 * outright once full - see shell.c's handling of that return), just gives
 * more headroom before it's hit in normal interactive use. */
#define MAX_PROCESSES 16
#define PROCESS_KERNEL_STACK_SIZE 8192
#define PROCESS_USER_STACK_PAGES 4
#define PROCESS_MAX_USER_PAGES 256

/* GDT segment selectors */
#define SEL_KERNEL_CS  0x08
#define SEL_KERNEL_DS  0x10
#define SEL_USER_DS    0x2B   /* 0x28 | RPL 3 */
#define SEL_USER_CS    0x33   /* 0x30 | RPL 3 */

/* User memory layout */
#define USER_CODE_BASE  0x00200000
#define USER_STACK_BASE 0x00800000
#define USER_STACK_SIZE (PROCESS_USER_STACK_PAGES * 4096)
#define USER_STACK_TOP  (USER_STACK_BASE + USER_STACK_SIZE)
#define USER_API_BASE   0x00090000
#define USER_ARGV_BASE  0x00091000

typedef enum {
    PROC_UNUSED = 0,
    PROC_LOADING,  /* process_create() returned it, but the caller hasn't
                    * finished mapping/writing its code, API, and argv pages
                    * yet - not schedulable until process_activate(). */
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_SLEEPING,
    PROC_TERMINATED
} process_state_t;

/* CPU context saved on kernel stack during interrupt/context switch.
 * Layout must match the push order in interrupt.asm irq0_handler. */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by CPU on interrupt */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) cpu_context_t;

typedef struct {
    uint64_t pid;
    process_state_t state;
    char name[32];

    uint64_t kernel_rsp;        /* Saved kernel stack pointer */
    uint8_t* kernel_stack;      /* Kernel stack base */
    uint64_t kernel_stack_top;  /* Kernel stack top (grows down) */

    uint64_t cr3;               /* Process page table */
    int is_user;                /* 1 = ring 3 process */
    int exit_code;
    int detached;               /* 1 = background process, reap automatically */
    uint64_t wait_target;
    uint64_t wake_tick;

    uint64_t user_pages[PROCESS_MAX_USER_PAGES];
    int user_page_count;
} process_t;

/* Process management */
void process_init(void);
process_t* process_create(const char* name, uint64_t entry_point, int is_user);
void process_terminate(uint64_t pid);
process_t* process_get_current(void);
process_t* process_get(uint64_t pid);
int process_is_running(uint64_t pid);
void process_exit(int exit_code);
void process_wait(uint64_t pid);
int process_waitpid(uint64_t pid, int* status, int options);
int process_list(process_t** list);
int process_reap_terminated(void);
void process_detach(uint64_t pid);
/* Marks a PROC_LOADING process schedulable. Callers (aob_spawn/elf_spawn)
 * must call this only after they've finished mapping and writing all of
 * the process's code/API/argv pages - see PROC_LOADING. */
void process_activate(uint64_t pid);
void process_sleep_ticks(uint64_t ticks);
int process_map_user_page(process_t* p, uint64_t virt, uint64_t flags);
int process_write_user(process_t* p, uint64_t virt, const void* data, uint64_t size);
int process_zero_user(process_t* p, uint64_t virt, uint64_t size);
int process_setup_user_args(process_t* p, const char* full_cmd);

/* Keyboard input ownership. There is one global PS/2 keyboard ring buffer
 * (keyboard.c) with no per-process queues, so with multiple processes able
 * to call ao_getchar()/ao_readline() concurrently (any backgrounded job),
 * keystrokes meant for whichever one the user is actually looking at could
 * otherwise be stolen by any other process also polling for input. Only
 * the foreground pid's SYS_GETCHAR/SYS_READLINE are allowed to consume
 * from the buffer (see syscall.c); everyone else just never reads from it.
 * Defaults to 0 (no user process - i.e. only the kernel/shell's own direct
 * keyboard_getchar() calls go through). Set/restored around a *foreground*
 * aob_spawn/elf_spawn wait; a background ('&') job is never made
 * foreground, so it can never consume input meant for the shell. */
void process_set_foreground(uint64_t pid);
int process_is_foreground(uint64_t pid);

/* Scheduler */
void scheduler_start(void);  /* Enable scheduling (call after boot completes) */
uint64_t schedule_tick(uint64_t current_rsp);  /* Called from timer IRQ */

#endif
