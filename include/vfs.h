#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "fs.h"

typedef struct {
    const char* name;
    int (*create)(const char* path, const char* data, uint32_t size);
    int (*read)(const char* path, char* buffer, uint32_t* size, uint32_t max_size);
    int (*delete)(const char* path);
    int (*list)(fs_file_info_t** files, int* count);
    int (*mkdir)(const char* path);
    int (*rmdir)(const char* path);
    int (*chdir)(const char* path);
    const char* (*getcwd)(void);
    void (*disk_info)(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb);
} vfs_ops_t;

void vfs_init(void);
int vfs_mount_root(const vfs_ops_t* ops);
int vfs_mount(const char* path, const vfs_ops_t* ops);
int vfs_umount(const char* path);
int vfs_get_mounts(char* buffer, uint32_t* size, uint32_t max_size);
const char* vfs_get_root_type(void);
int vfs_create(const char* path, const char* data, uint32_t size);
int vfs_read(const char* path, char* buffer, uint32_t* size, uint32_t max_size);
int vfs_delete(const char* path);
int vfs_list(fs_file_info_t** files, int* count);
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);
int vfs_chdir(const char* path);
const char* vfs_getcwd(void);
void vfs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb);

#endif
