#ifndef FAT_H
#define FAT_H

#include <stdint.h>

typedef enum {
    FAT_TYPE_NONE = 0,
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32
} fat_type_t;

void fat_init(void);
int fat_available(void);
fat_type_t fat_get_type(void);
int fat_create(const char* name, const char* data, uint32_t size);
int fat_read(const char* name, char* buffer, uint32_t* size, uint32_t max_size);
int fat_delete(const char* name);
int fat_list(void** entries, int* count);
int fat_mkdir(const char* name);
int fat_rmdir(const char* name);
int fat_chdir(const char* name);
const char* fat_getcwd(void);
void fat_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb);

#endif
