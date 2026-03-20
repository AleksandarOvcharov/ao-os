#include "cpu.h"
#include "pmm.h"
#include "string.h"
#include "klog.h"

static cpu_info_t cpu_info;

static inline void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx,
                          uint32_t* ecx, uint32_t* edx) {
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "a"(leaf), "c"(0));
}

void cpu_detect(void) {
    uint32_t eax, ebx, ecx, edx;

    memset(&cpu_info, 0, sizeof(cpu_info));

    /* Leaf 0: Vendor string and max standard leaf */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    cpu_info.max_leaf = eax;
    *((uint32_t*)(cpu_info.vendor + 0)) = ebx;
    *((uint32_t*)(cpu_info.vendor + 4)) = edx;
    *((uint32_t*)(cpu_info.vendor + 8)) = ecx;
    cpu_info.vendor[12] = '\0';

    /* Leaf 1: Family, model, stepping, features */
    if (cpu_info.max_leaf >= 1) {
        cpuid(1, &eax, &ebx, &ecx, &edx);
        cpu_info.stepping = eax & 0xF;
        cpu_info.model = (eax >> 4) & 0xF;
        cpu_info.family = (eax >> 8) & 0xF;

        /* Extended model/family for families >= 6 */
        if (cpu_info.family == 6 || cpu_info.family == 15) {
            cpu_info.model |= ((eax >> 16) & 0xF) << 4;
        }
        if (cpu_info.family == 15) {
            cpu_info.family += (eax >> 20) & 0xFF;
        }

        cpu_info.features_edx = edx;
        cpu_info.features_ecx = ecx;
    }

    /* Extended leaf 0x80000000: Max extended leaf */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    cpu_info.max_ext_leaf = eax;

    /* Extended leaf 0x80000001: Extended features */
    if (cpu_info.max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        cpu_info.ext_features = edx;
    }

    /* Extended leaves 0x80000002-4: Brand string (48 chars) */
    if (cpu_info.max_ext_leaf >= 0x80000004) {
        uint32_t* brand = (uint32_t*)cpu_info.brand;
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        cpu_info.brand[48] = '\0';
    } else {
        /* No brand string, copy vendor */
        for (int i = 0; i < 12; i++)
            cpu_info.brand[i] = cpu_info.vendor[i];
        cpu_info.brand[12] = '\0';
    }

    klog_info("CPU detected");
}

const cpu_info_t* cpu_get_info(void) {
    return &cpu_info;
}

/* Legacy functions for backward compatibility */
void cpu_get_vendor(char* vendor) {
    for (int i = 0; i < 13; i++)
        vendor[i] = cpu_info.vendor[i];
}

uint32_t cpu_detect_memory(void) {
    return (uint32_t)(pmm_get_total_memory() / 1024);
}
