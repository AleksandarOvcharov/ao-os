#include "cpu.h"
#include "memory.h"

void cpu_get_vendor(char* vendor) {
    uint32_t ebx, ecx, edx;
    
    asm volatile(
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    
    *((uint32_t*)(vendor + 0)) = ebx;
    *((uint32_t*)(vendor + 4)) = edx;
    *((uint32_t*)(vendor + 8)) = ecx;
    vendor[12] = '\0';
}

uint32_t cpu_detect_memory(void) {
    return HEAP_SIZE / 1024;
}
