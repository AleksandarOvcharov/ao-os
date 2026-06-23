#ifndef FAT32_H
#define FAT32_H
#include "fat.h"
static inline int fat32_available(void) { return fat_get_type() == FAT_TYPE_32; }
#endif
