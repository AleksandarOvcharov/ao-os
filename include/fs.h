#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAX_FILENAME 32
#define FS_MAX_FILESIZE 512

typedef struct {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    uint8_t used;
} fs_file_info_t;

void fs_init(void);
int fs_create(const char* name, const char* data, uint32_t size);
int fs_read(const char* name, char* buffer, uint32_t* size);
int fs_delete(const char* name);
int fs_list(fs_file_info_t** files, int* count);

#endif
