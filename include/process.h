#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 8
#define PROCESS_KERNEL_STACK_SIZE 8192
#define PROCESS_USER_STACK_PAGES 4

/* GDT segment selectors */
#define SEL_KERNEL_CS  0x08
#define SEL_KERNEL_DS  0x10
#define SEL_USER_DS    0x2B   /* 0x28 | RPL 3 */
#define SEL_USER_CS    0x33   /* 0x30 | RPL 3 */

/* User memory layout (identity-mapped) */
#define USER_CODE_BASE  0x00200000
#define USER_STACK_BASE 0x00800000
#define USER_STACK_SIZE (PROCESS_USER_STACK_PAGES * 4096)
#define USER_STACK_TOP  (USER_STACK_BASE + USER_STACK_SIZE)
#define USER_API_BASE   0x00090000
#define USER_ARGV_BASE  0x00091000

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
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

    uint64_t cr3;               /* Page table (shared for now) */
    int is_user;                /* 1 = ring 3 process */
    int exit_code;
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
int process_list(process_t** list);

/* Scheduler */
void scheduler_start(void);  /* Enable scheduling (call after boot completes) */
uint64_t schedule_tick(uint64_t current_rsp);  /* Called from timer IRQ */

#endif
