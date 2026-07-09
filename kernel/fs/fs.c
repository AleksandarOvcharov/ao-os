#include "fs.h"
#include "ramfs.h"
#include "fat.h"
#include "vfs.h"
#include "block_cache.h"
#include "klog.h"

static int fatfs_list(fs_file_info_t** files, int* count) {
    void* fat_entries;
    int fat_count;
    if (fat_list(&fat_entries, &fat_count) == 0) {
        *files = (fs_file_info_t*)fat_entries;
        *count = fat_count;
        return 0;
    }
    return -1;
}

static const char* fatfs_name(void) {
    if (fat_get_type() == FAT_TYPE_16) return "FAT16";
    if (fat_get_type() == FAT_TYPE_32) return "FAT32";
    return "FAT12";
}

static int ramfs_list_adapter(fs_file_info_t** files, int* count) {
    *count = ramfs_get_file_count();
    *files = (fs_file_info_t*)ramfs_get_files();
    return 0;
}

static void ramfs_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    *total_kb = 0;
    *used_kb = 0;
    *free_kb = 0;
}

static vfs_ops_t fat_ops = {
    "FAT",
    fat_create, fat_read, fat_delete, fatfs_list,
    fat_mkdir, fat_rmdir, fat_chdir, fat_getcwd, fat_get_disk_info
};

static const vfs_ops_t ramfs_ops = {
    "ramfs",
    ramfs_create, ramfs_read, ramfs_delete, ramfs_list_adapter,
    ramfs_mkdir, ramfs_rmdir, ramfs_chdir, ramfs_getcwd, ramfs_disk_info
};

void fs_init(void) {
    klog_info("Initializing VFS and filesystem layer...");

    block_cache_init();
    vfs_init();
    fat_init();

    if (fat_available()) {
        fat_ops.name = fatfs_name();
        vfs_mount_root(&fat_ops);
        ramfs_init();
        vfs_mount("/dev", &ramfs_ops);
        klog_info("VFS mounted FAT root filesystem");
    } else {
        ramfs_init();
        vfs_mount_root(&ramfs_ops);
        vfs_mount("/dev", &ramfs_ops);
        klog_info("VFS mounted ramfs root filesystem");
    }
}

int fs_create(const char* name, const char* data, uint32_t size) {
    return vfs_create(name, data, size);
}

int fs_read(const char* name, char* buffer, uint32_t* size) {
    return vfs_read(name, buffer, size);
}

int fs_delete(const char* name) {
    return vfs_delete(name);
}

int fs_list(fs_file_info_t** files, int* count) {
    return vfs_list(files, count);
}

const char* fs_get_type(void) {
    return vfs_get_root_type();
}

int fs_mkdir(const char* name) {
    return vfs_mkdir(name);
}

int fs_rmdir(const char* name) {
    return vfs_rmdir(name);
}

int fs_chdir(const char* name) {
    return vfs_chdir(name);
}

const char* fs_getcwd(void) {
    return vfs_getcwd();
}

void fs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    vfs_get_disk_info(total_kb, used_kb, free_kb);
}
