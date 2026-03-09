#ifndef AO_H
#define AO_H

// AO-OS User Program API
// Uses kernel function pointers at 0x00090000

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long size_t;

// VGA Colors
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

// Kernel API at fixed address 0x00090000
// Layout: [magic][putchar_ptr][writestring_ptr][setcolor_ptr][clear_ptr]
#define AO_API_ADDR 0x00090000
#define AO_API_MAGIC 0x414F4150

static inline void ao_putchar(char c) {
    volatile unsigned int* api = (volatile unsigned int*)AO_API_ADDR;
    if (api[0] == AO_API_MAGIC) {
        void (*fn)(char) = (void (*)(char))api[1];
        fn(c);
    }
}

static inline void ao_print(const char* str) {
    volatile unsigned int* api = (volatile unsigned int*)AO_API_ADDR;
    if (api[0] == AO_API_MAGIC) {
        void (*fn)(const char*) = (void (*)(const char*))api[2];
        fn(str);
    }
}

static inline void ao_setcolor(enum vga_color fg, enum vga_color bg) {
    volatile unsigned int* api = (volatile unsigned int*)AO_API_ADDR;
    if (api[0] == AO_API_MAGIC) {
        void (*fn)(uint8_t) = (void (*)(uint8_t))api[3];
        fn((uint8_t)(fg | (bg << 4)));
    }
}

static inline void ao_clear(void) {
    volatile unsigned int* api = (volatile unsigned int*)AO_API_ADDR;
    if (api[0] == AO_API_MAGIC) {
        void (*fn)(void) = (void (*)(void))api[4];
        fn();
    }
}

static inline void ao_println(const char* str) {
    ao_print(str);
    ao_putchar('\n');
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
