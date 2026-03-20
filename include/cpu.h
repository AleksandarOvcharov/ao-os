#ifndef CPU_H
#define CPU_H

#include <stdint.h>

/* CPU feature flags (from CPUID leaf 1, EDX) */
#define CPU_FEAT_FPU    (1 << 0)
#define CPU_FEAT_PSE    (1 << 3)
#define CPU_FEAT_TSC    (1 << 4)
#define CPU_FEAT_MSR    (1 << 5)
#define CPU_FEAT_PAE    (1 << 6)
#define CPU_FEAT_APIC   (1 << 9)
#define CPU_FEAT_SSE    (1 << 25)
#define CPU_FEAT_SSE2   (1 << 26)

/* CPU feature flags (from CPUID leaf 1, ECX) */
#define CPU_FEAT_SSE3   (1 << 0)
#define CPU_FEAT_SSE41  (1 << 19)
#define CPU_FEAT_SSE42  (1 << 20)
#define CPU_FEAT_AVX    (1 << 28)

/* Extended feature flags (from CPUID leaf 0x80000001, EDX) */
#define CPU_EXT_NX      (1 << 20)
#define CPU_EXT_LM      (1 << 29)   /* Long Mode (64-bit) */

typedef struct {
    char vendor[13];
    char brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t features_edx;    /* CPUID leaf 1, EDX */
    uint32_t features_ecx;    /* CPUID leaf 1, ECX */
    uint32_t ext_features;    /* CPUID leaf 0x80000001, EDX */
    uint32_t max_leaf;
    uint32_t max_ext_leaf;
} cpu_info_t;

void cpu_detect(void);
const cpu_info_t* cpu_get_info(void);
void cpu_get_vendor(char* vendor);
uint32_t cpu_detect_memory(void);

#endif
