#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAX_FILENAME 256
#define FS_MAX_FILESIZE 16384  // 32 clusters * 512 bytes

typedef struct {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    uint8_t used;
    uint8_t is_directory;
} fs_file_info_t;

void fs_init(void);
int fs_create(const char* name, const char* data, uint32_t size);
int fs_read(const char* name, char* buffer, uint32_t* size);
int fs_delete(const char* name);
int fs_list(fs_file_info_t** files, int* count);
const char* fs_get_type(void);
int fs_mkdir(const char* name);
int fs_rmdir(const char* name);
int fs_chdir(const char* name);
const char* fs_getcwd(void);
void fs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb);

#endif
