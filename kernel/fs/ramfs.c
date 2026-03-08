#include "ramfs.h"
#include "string.h"
#include "klog.h"

static ramfs_file_t files[MAX_FILES];
static int file_count = 0;

void ramfs_init(void) {
    klog_info("Initializing RAM filesystem...");
    
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
        files[i].size = 0;
        memset(files[i].name, 0, MAX_FILENAME);
        memset(files[i].data, 0, MAX_FILESIZE);
    }
    
    file_count = 0;
    klog_info("RAM filesystem initialized");
}

int ramfs_create(const char* name, const char* data, uint32_t size) {
    if (size > MAX_FILESIZE) {
        return -1;
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            memset(files[i].data, 0, MAX_FILESIZE);
            memcpy(files[i].data, data, size);
            files[i].size = size;
            if (size < MAX_FILESIZE) {
                files[i].data[size] = '\0';
            }
            return 0;
        }
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            strncpy(files[i].name, name, MAX_FILENAME - 1);
            files[i].name[MAX_FILENAME - 1] = '\0';
            memcpy(files[i].data, data, size);
            files[i].size = size;
            if (size < MAX_FILESIZE) {
                files[i].data[size] = '\0';
            }
            file_count++;
            return 0;
        }
    }
    
    return -1;
}

int ramfs_read(const char* name, char* buffer, uint32_t* size) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            memcpy(buffer, files[i].data, files[i].size);
            *size = files[i].size;
            return 0;
        }
    }
    
    return -1;
}

int ramfs_delete(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            files[i].used = 0;
            files[i].size = 0;
            memset(files[i].name, 0, MAX_FILENAME);
            memset(files[i].data, 0, MAX_FILESIZE);
            file_count--;
            return 0;
        }
    }
    
    return -1;
}

int ramfs_list(void) {
    return file_count;
}

ramfs_file_t* ramfs_get_files(void) {
    return files;
}

int ramfs_get_file_count(void) {
    return file_count;
}
