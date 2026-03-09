#include "fs.h"
#include "ramfs.h"
#include "fat12.h"
#include "klog.h"

static int use_fat12 = 0;

void fs_init(void) {
    klog_info("Initializing filesystem abstraction layer...");
    
    fat12_init();
    
    if (fat12_available()) {
        use_fat12 = 1;
        klog_info("Filesystem abstraction layer initialized (using FAT12)");
    } else {
        use_fat12 = 0;
        ramfs_init();
        klog_info("Filesystem abstraction layer initialized (using ramfs)");
    }
}

int fs_create(const char* name, const char* data, uint32_t size) {
    if (use_fat12) {
        int result = fat12_create(name, data, size);
        if (result == -1) {
            return ramfs_create(name, data, size);
        }
        return result;
    }
    return ramfs_create(name, data, size);
}

int fs_read(const char* name, char* buffer, uint32_t* size) {
    if (use_fat12) {
        int result = fat12_read(name, buffer, size);
        if (result == 0) return 0;
    }
    return ramfs_read(name, buffer, size);
}

int fs_delete(const char* name) {
    if (use_fat12) {
        int result = fat12_delete(name);
        if (result == -1) {
            return ramfs_delete(name);
        }
        return result;
    }
    return ramfs_delete(name);
}

int fs_list(fs_file_info_t** files, int* count) {
    if (use_fat12) {
        void* fat_entries;
        int fat_count;
        if (fat12_list(&fat_entries, &fat_count) == 0) {
            *files = (fs_file_info_t*)fat_entries;
            *count = fat_count;
            return 0;
        }
    }
    
    ramfs_file_t* ramfs_files = ramfs_get_files();
    *count = ramfs_get_file_count();
    *files = (fs_file_info_t*)ramfs_files;
    return 0;
}

const char* fs_get_type(void) {
    if (use_fat12) {
        return "FAT12";
    }
    return "ramfs";
}

int fs_mkdir(const char* name) {
    if (use_fat12) {
        return fat12_mkdir(name);
    }
    return ramfs_mkdir(name);
}

int fs_rmdir(const char* name) {
    if (use_fat12) {
        return fat12_rmdir(name);
    }
    return ramfs_rmdir(name);
}

int fs_chdir(const char* name) {
    if (use_fat12) {
        return fat12_chdir(name);
    }
    return ramfs_chdir(name);
}

const char* fs_getcwd(void) {
    if (use_fat12) {
        return fat12_getcwd();
    }
    return ramfs_getcwd();
}

void fs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    if (use_fat12) {
        fat12_get_disk_info(total_kb, used_kb, free_kb);
    } else {
        *total_kb = 0;
        *used_kb  = 0;
        *free_kb  = 0;
    }
}
