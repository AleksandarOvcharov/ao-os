#ifndef INSTALLER_H
#define INSTALLER_H

#include <stdint.h>

// Installer status codes
#define INSTALLER_OK 0
#define INSTALLER_NO_DISK -1
#define INSTALLER_WRITE_ERROR -2
#define INSTALLER_READ_ERROR -3
#define INSTALLER_UNSUPPORTED -4

typedef enum {
    INSTALLER_FS_FAT12 = 12,
    INSTALLER_FS_FAT16 = 16,
    INSTALLER_FS_FAT32 = 32
} installer_fs_t;

// Installer functions
int installer_detect_disk(void);
int installer_prepare_disk(void);
int installer_format_filesystem(void);
int installer_format_filesystem_as(installer_fs_t type);
int installer_install_kernel(void);
int installer_install_bootloader(void);
int installer_run(void);

#endif
