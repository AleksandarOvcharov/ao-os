#include "syscall.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"
#include "timer.h"
#include "version.h"
#include "process.h"
#include "vmm.h"

/* Longest NUL-terminated string (filename, print message, ...) a syscall
 * will scan for from user memory before giving up. */
#define SYSCALL_MAX_STR 4096

/* True if [addr, addr+len) lies entirely in memory the calling user process
 * can itself read/write. Must be checked before the kernel dereferences any
 * pointer a syscall receives from ring 3 - otherwise a process can hand in
 * a kernel address and use the syscall as a read/write gadget into kernel
 * memory. A zero-length range is accepted without touching any page. */
static int user_buf_ok(uint64_t addr, uint64_t len) {
    if (len == 0) return 1;
    process_t* p = process_get_current();
    if (!p || !p->is_user) return 0;
    return vmm_user_range_ok(p->cr3, addr, len);
}

/* Validates a NUL-terminated user string and returns its length, or -1 if
 * it isn't fully readable by the calling process within SYSCALL_MAX_STR. */
static long user_str_len(uint64_t addr) {
    process_t* p = process_get_current();
    if (!p || !p->is_user) return -1;
    return vmm_user_strlen(p->cr3, addr, SYSCALL_MAX_STR);
}

// Syscall numbers - must match ao.h
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
#define SYS_WAITPID     14
#define SYS_SLEEP       15

// Simple file handle table, shared across all processes (max 8 open files
// system-wide) but each slot is tagged with the process that owns it, so
// one process can't read/close a handle another process opened.
#define MAX_FD 8

static struct {
    char name[256];
    char data[16384];
    uint32_t size;
    uint32_t pos;
    int used;
    uint64_t owner_pid;
} fd_table[MAX_FD];

static void fd_table_init(void) {
    for (int i = 0; i < MAX_FD; i++) {
        fd_table[i].used = 0;
        fd_table[i].pos = 0;
        fd_table[i].owner_pid = 0;
    }
}

void syscall_close_process_fds(uint64_t pid) {
    for (int i = 0; i < MAX_FD; i++) {
        if (fd_table[i].used && fd_table[i].owner_pid == pid) {
            fd_table[i].used = 0;
        }
    }
}

int syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg3;

    switch (num) {
        case SYS_PRINT:
            if (!arg1) return 0;
            if (user_str_len(arg1) < 0) return -1;
            terminal_writestring((const char*)(uintptr_t)arg1);
            return 0;

        case SYS_PUTCHAR:
            terminal_putchar((char)arg1);
            return 0;

        case SYS_CLEAR:
            terminal_clear();
            return 0;

        case SYS_GETCHAR: {
            process_t* self = process_get_current();
            uint64_t pid = self ? self->pid : 0;
            unsigned char c = 0;
            while (c == 0) {
                if (process_is_foreground(pid)) c = keyboard_getchar();
            }
            return (int)c;
        }

        case SYS_READLINE: {
            char* buf = (char*)(uintptr_t)arg1;
            uint32_t maxlen = (uint32_t)arg2;
            if (!buf || maxlen == 0) return -1;
            if (!user_buf_ok(arg1, maxlen)) return -1;
            process_t* self = process_get_current();
            uint64_t pid = self ? self->pid : 0;
            uint32_t i = 0;
            while (i < maxlen - 1) {
                unsigned char c = 0;
                while (c == 0) {
                    if (process_is_foreground(pid)) c = keyboard_getchar();
                }
                if (c == '\n' || c == '\r') {
                    terminal_putchar('\n');
                    break;
                } else if (c == '\b') {
                    if (i > 0) {
                        i--;
                        terminal_backspace();
                    }
                } else if (c >= 32 && c < 128) {
                    buf[i++] = (char)c;
                    terminal_putchar((char)c);
                }
            }
            buf[i] = '\0';
            return (int)i;
        }

        case SYS_EXIT:
            process_exit((int)arg1);
            /* Never reached */
            return 0;

        case SYS_UPTIME:
            return (int)timer_get_ticks();

        case SYS_SYSINFO: {
            char* buf = (char*)(uintptr_t)arg1;
            uint32_t maxlen = (uint32_t)arg2;
            if (!buf || maxlen < 2) return -1;  /* maxlen-2 below would underflow otherwise */
            if (!user_buf_ok(arg1, maxlen)) return -1;
            const char* name = "AO-OS";
            const char* ver  = KERNEL_VERSION_STRING;
            uint32_t i = 0;
            while (*name && i < maxlen - 2) buf[i++] = *name++;
            buf[i++] = ' ';
            while (*ver  && i < maxlen - 1) buf[i++] = *ver++;
            buf[i] = '\0';
            return (int)i;
        }

        case SYS_OPEN: {
            const char* name = (const char*)(uintptr_t)arg1;
            if (!name) return -1;
            if (user_str_len(arg1) < 0) return -1;
            process_t* opener = process_get_current();
            if (!opener) return -1;
            for (int i = 0; i < MAX_FD; i++) {
                if (!fd_table[i].used) {
                    uint32_t sz = 0;
                    if (fs_read(name, fd_table[i].data, &sz, sizeof(fd_table[i].data)) != 0) return -1;
                    fd_table[i].size = sz;
                    fd_table[i].pos  = 0;
                    fd_table[i].used = 1;
                    fd_table[i].owner_pid = opener->pid;
                    int nlen = 0;
                    while (name[nlen] && nlen < 255) { fd_table[i].name[nlen] = name[nlen]; nlen++; }
                    fd_table[i].name[nlen] = '\0';
                    return i;
                }
            }
            return -1;
        }

        case SYS_READ: {
            int fd = (int)arg1;
            char* buf = (char*)(uintptr_t)arg2;
            uint32_t len = (uint32_t)arg3;
            process_t* reader = process_get_current();
            if (fd < 0 || fd >= MAX_FD || !fd_table[fd].used || !buf) return -1;
            if (!reader || fd_table[fd].owner_pid != reader->pid) return -1;
            if (len > 0 && !user_buf_ok(arg2, len)) return -1;
            if (fd_table[fd].pos >= fd_table[fd].size) return 0;
            uint32_t remaining = fd_table[fd].size - fd_table[fd].pos;
            if (len > remaining) len = remaining;
            for (uint32_t i = 0; i < len; i++)
                buf[i] = fd_table[fd].data[fd_table[fd].pos + i];
            fd_table[fd].pos += len;
            return (int)len;
        }

        case SYS_WRITE: {
            const char* buf = (const char*)(uintptr_t)arg1;
            uint32_t len = (uint32_t)arg2;
            if (!buf) return -1;
            if (len > 0 && !user_buf_ok(arg1, len)) return -1;
            for (uint32_t i = 0; i < len; i++)
                terminal_putchar(buf[i]);
            return (int)len;
        }

        case SYS_CLOSE: {
            int fd = (int)arg1;
            process_t* closer = process_get_current();
            if (fd < 0 || fd >= MAX_FD || !fd_table[fd].used) return -1;
            if (!closer || fd_table[fd].owner_pid != closer->pid) return -1;
            fd_table[fd].used = 0;
            return 0;
        }

        case SYS_SETCOLOR:
            terminal_setcolor((uint8_t)arg1);
            return 0;

        case SYS_WAITPID:
            if (arg2 && !user_buf_ok(arg2, sizeof(int))) return -1;
            return process_waitpid(arg1, (int*)(uintptr_t)arg2, (int)arg3);

        case SYS_SLEEP:
            process_sleep_ticks(arg1);
            return 0;

        default:
            return -1;
    }
}

void syscall_init(void) {
    fd_table_init();
}
