#ifndef FAT16_H
#define FAT16_H
#include "fat.h"
static inline int fat16_available(void) { return fat_get_type() == FAT_TYPE_16; }
#endif
