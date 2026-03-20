#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// Syscall numbers
#define SYS_PRINT       1
#define SYS_PUTCHAR     2
#define SYS_CLEAR       3
#define SYS_GETCHAR     4
#define SYS_READLINE    5
#define SYS_EXIT        6
#define SYS_UPTIME      7
#define SYS_SYSINFO     8
#define SYS_OPEN        9
#define SYS_READ        10
#define SYS_WRITE       11
#define SYS_CLOSE       12
#define SYS_SETCOLOR    13

void syscall_init(void);
int  syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif
