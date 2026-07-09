#include "vfs.h"

static const vfs_ops_t* root_fs;

void vfs_init(void) {
    root_fs = 0;
}

int vfs_mount_root(const vfs_ops_t* ops) {
    if (!ops) return -1;
    root_fs = ops;
    return 0;
}

const char* vfs_get_root_type(void) {
    return root_fs ? root_fs->name : "none";
}

int vfs_create(const char* path, const char* data, uint32_t size) {
    return root_fs && root_fs->create ? root_fs->create(path, data, size) : -1;
}

int vfs_read(const char* path, char* buffer, uint32_t* size) {
    return root_fs && root_fs->read ? root_fs->read(path, buffer, size) : -1;
}

int vfs_delete(const char* path) {
    return root_fs && root_fs->delete ? root_fs->delete(path) : -1;
}

int vfs_list(fs_file_info_t** files, int* count) {
    return root_fs && root_fs->list ? root_fs->list(files, count) : -1;
}

int vfs_mkdir(const char* path) {
    return root_fs && root_fs->mkdir ? root_fs->mkdir(path) : -1;
}

int vfs_rmdir(const char* path) {
    return root_fs && root_fs->rmdir ? root_fs->rmdir(path) : -1;
}

int vfs_chdir(const char* path) {
    return root_fs && root_fs->chdir ? root_fs->chdir(path) : -1;
}

const char* vfs_getcwd(void) {
    return root_fs && root_fs->getcwd ? root_fs->getcwd() : "/";
}

void vfs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    if (root_fs && root_fs->disk_info) {
        root_fs->disk_info(total_kb, used_kb, free_kb);
    } else {
        *total_kb = 0;
        *used_kb = 0;
        *free_kb = 0;
    }
}
