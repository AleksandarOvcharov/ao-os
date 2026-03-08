#include "fs.h"
#include "ramfs.h"
#include "klog.h"

void fs_init(void) {
    klog_info("Initializing filesystem abstraction layer...");
    ramfs_init();
    klog_info("Filesystem abstraction layer initialized (using ramfs)");
}

int fs_create(const char* name, const char* data, uint32_t size) {
    return ramfs_create(name, data, size);
}

int fs_read(const char* name, char* buffer, uint32_t* size) {
    return ramfs_read(name, buffer, size);
}

int fs_delete(const char* name) {
    return ramfs_delete(name);
}

int fs_list(fs_file_info_t** files, int* count) {
    ramfs_file_t* ramfs_files = ramfs_get_files();
    *count = ramfs_get_file_count();
    *files = (fs_file_info_t*)ramfs_files;
    return 0;
}
