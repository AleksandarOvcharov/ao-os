#include "fs.h"
#include "ramfs.h"
#include "fat.h"
#include "klog.h"

static int use_fat = 0;

void fs_init(void) {
    klog_info("Initializing filesystem abstraction layer...");
    
    fat_init();
    
    if (fat_available()) {
        use_fat = 1;
        klog_info("Filesystem abstraction layer initialized (using FAT)");
    } else {
        use_fat = 0;
        ramfs_init();
        klog_info("Filesystem abstraction layer initialized (using ramfs)");
    }
}

int fs_create(const char* name, const char* data, uint32_t size) {
    if (use_fat) {
        return fat_create(name, data, size);
    }
    return ramfs_create(name, data, size);
}

int fs_read(const char* name, char* buffer, uint32_t* size) {
    if (use_fat) {
        return fat_read(name, buffer, size);
    }
    return ramfs_read(name, buffer, size);
}

int fs_delete(const char* name) {
    if (use_fat) {
        return fat_delete(name);
    }
    return ramfs_delete(name);
}

int fs_list(fs_file_info_t** files, int* count) {
    if (use_fat) {
        void* fat_entries;
        int fat_count;
        if (fat_list(&fat_entries, &fat_count) == 0) {
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
    if (use_fat) {
        if (fat_get_type() == FAT_TYPE_16) return "FAT16";
        if (fat_get_type() == FAT_TYPE_32) return "FAT32";
        return "FAT12";
    }
    return "ramfs";
}

int fs_mkdir(const char* name) {
    if (use_fat) {
        return fat_mkdir(name);
    }
    return ramfs_mkdir(name);
}

int fs_rmdir(const char* name) {
    if (use_fat) {
        return fat_rmdir(name);
    }
    return ramfs_rmdir(name);
}

int fs_chdir(const char* name) {
    if (use_fat) {
        return fat_chdir(name);
    }
    return ramfs_chdir(name);
}

const char* fs_getcwd(void) {
    if (use_fat) {
        return fat_getcwd();
    }
    return ramfs_getcwd();
}

void fs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    if (use_fat) {
        fat_get_disk_info(total_kb, used_kb, free_kb);
    } else {
        *total_kb = 0;
        *used_kb  = 0;
        *free_kb  = 0;
    }
}
