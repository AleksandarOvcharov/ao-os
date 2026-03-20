#ifndef AO_H
#define AO_H

// AO-OS User Program API
// Uses int 0x80 syscalls: rax=num, rbx=arg1, rcx=arg2, rdx=arg3

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef unsigned long size_t;
typedef unsigned long uintptr_t;

// Syscall numbers (must match kernel/syscall.c)
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

// Colors
enum vga_color {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN = 6,
    COLOR_LIGHT_GREY = 7,
    COLOR_DARK_GREY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_YELLOW = 14,
    COLOR_WHITE = 15,
};

// Raw syscall: rax=num, rbx=arg1, rcx=arg2, rdx=arg3, returns rax
static inline int syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

// Console
static inline void ao_print(const char* str) {
    syscall(SYS_PRINT, (uintptr_t)str, 0, 0);
}

static inline void ao_putchar(char c) {
    syscall(SYS_PUTCHAR, (uint64_t)c, 0, 0);
}

static inline void ao_clear(void) {
    syscall(SYS_CLEAR, 0, 0, 0);
}

static inline void ao_println(const char* str) {
    ao_print(str);
    ao_putchar('\n');
}

static inline void ao_setcolor(enum vga_color fg, enum vga_color bg) {
    syscall(SYS_SETCOLOR, (uint64_t)((int)fg | ((int)bg << 4)), 0, 0);
}

// Keyboard
static inline int ao_getchar(void) {
    return syscall(SYS_GETCHAR, 0, 0, 0);
}

static inline int ao_readline(char* buf, uint32_t maxlen) {
    return syscall(SYS_READLINE, (uintptr_t)buf, maxlen, 0);
}

// Process
static inline void ao_exit(int code) {
    syscall(SYS_EXIT, (uint64_t)code, 0, 0);
}

// System
static inline int ao_uptime(void) {
    return syscall(SYS_UPTIME, 0, 0, 0);
}

static inline int ao_sysinfo(char* buf, uint32_t maxlen) {
    return syscall(SYS_SYSINFO, (uintptr_t)buf, maxlen, 0);
}

// Files
static inline int ao_open(const char* name) {
    return syscall(SYS_OPEN, (uintptr_t)name, 0, 0);
}

static inline int ao_read(int fd, char* buf, uint32_t len) {
    return syscall(SYS_READ, (uint64_t)fd, (uintptr_t)buf, len);
}

static inline int ao_write(const char* buf, uint32_t len) {
    return syscall(SYS_WRITE, (uintptr_t)buf, len, 0);
}

static inline int ao_close(int fd) {
    return syscall(SYS_CLOSE, (uint64_t)fd, 0, 0);
}

// argc/argv - written by shell before execution at 0x00091000
// Layout: [uint64_t argc][uint64_t argv[0]..argv[15]][strings...]
#define AO_ARGV_BASE 0x00091000
#define AO_ARGV_MAX  16

static inline int ao_argc(void) {
    return (int)(*(volatile uint64_t*)AO_ARGV_BASE);
}

static inline const char* ao_argv(int i) {
    if (i < 0 || i >= ao_argc()) return (const char*)0;
    volatile uint64_t* base = (volatile uint64_t*)AO_ARGV_BASE;
    return (const char*)base[1 + i];
}

// String length
static inline size_t ao_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

// String copy
static inline void ao_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// String compare
static inline int ao_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Memory set
static inline void ao_memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
}

// Memory copy
static inline void ao_memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (num--) {
        *d++ = *s++;
    }
}

// Integer to string
static inline void ao_itoa(int value, char* str) {
    int i = 0;
    int is_negative = 0;

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    if (value == 0) {
        str[i++] = '0';
    } else {
        char temp[12];
        int j = 0;
        while (value > 0) {
            temp[j++] = '0' + (value % 10);
            value /= 10;
        }
        if (is_negative) {
            str[i++] = '-';
        }
        while (j > 0) {
            str[i++] = temp[--j];
        }
    }
    str[i] = '\0';
}

// Print integer
static inline void ao_printint(int value) {
    char buffer[12];
    ao_itoa(value, buffer);
    ao_print(buffer);
}

// Delay (busy wait)
static inline void ao_delay(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}

#endif
