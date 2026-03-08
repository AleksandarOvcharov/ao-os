#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define MAX_FILES 16
#define MAX_FILENAME 32
#define MAX_FILESIZE 512

typedef struct {
    char name[MAX_FILENAME];
    char data[MAX_FILESIZE];
    uint32_t size;
    uint8_t used;
} ramfs_file_t;

void ramfs_init(void);
int ramfs_create(const char* name, const char* data, uint32_t size);
int ramfs_read(const char* name, char* buffer, uint32_t* size);
int ramfs_delete(const char* name);
int ramfs_list(void);
ramfs_file_t* ramfs_get_files(void);
int ramfs_get_file_count(void);
int ramfs_mkdir(const char* name);
int ramfs_rmdir(const char* name);
int ramfs_chdir(const char* name);
const char* ramfs_getcwd(void);

#endif
