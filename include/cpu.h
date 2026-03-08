#ifndef CPU_H
#define CPU_H

#include <stdint.h>

void cpu_get_vendor(char* vendor);
uint32_t cpu_detect_memory(void);

#endif
