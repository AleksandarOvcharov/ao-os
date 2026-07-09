#include "fs.h"
#include "ramfs.h"
#include "fat.h"
#include "vfs.h"
#include "block_cache.h"
#include "klog.h"
#include "process.h"
#include "string.h"
#include "net.h"

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

static fs_file_info_t proc_entries[2];
static fs_file_info_t dev_entries[2];

static void append_str(char* b, uint32_t* p, uint32_t max, const char* s) {
    while (*s && *p + 1 < max) b[(*p)++] = *s++;
}

static void append_u64(char* b, uint32_t* p, uint32_t max, uint64_t v) {
    char tmp[24]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0 && *p + 1 < max) b[(*p)++] = tmp[--n];
}

static const char* proc_state_name(process_state_t s) {
    switch (s) {
        case PROC_READY: return "READY";
        case PROC_RUNNING: return "RUNNING";
        case PROC_BLOCKED: return "BLOCKED";
        case PROC_SLEEPING: return "SLEEPING";
        case PROC_TERMINATED: return "TERMINATED";
        default: return "UNUSED";
    }
}

static int proc_read(const char* path, char* buffer, uint32_t* size) {
    if (!path) return -1;
    if (strcmp(path, "net/arp") == 0) return net_format_arp_cache(buffer, size, FS_MAX_FILESIZE);
    if (strcmp(path, "net/dev") == 0) return net_format_device_stats(buffer, size, FS_MAX_FILESIZE);
    if (strcmp(path, "processes") != 0) return -1;
    uint32_t pos = 0;
    process_t* list = 0;
    int count = process_list(&list);
    append_str(buffer, &pos, FS_MAX_FILESIZE, "PID STATE NAME\n");
    for (int i = 0; i < count; i++) {
        if (list[i].state == PROC_UNUSED) continue;
        append_u64(buffer, &pos, FS_MAX_FILESIZE, list[i].pid);
        append_str(buffer, &pos, FS_MAX_FILESIZE, " ");
        append_str(buffer, &pos, FS_MAX_FILESIZE, proc_state_name(list[i].state));
        append_str(buffer, &pos, FS_MAX_FILESIZE, " ");
        append_str(buffer, &pos, FS_MAX_FILESIZE, list[i].name);
        append_str(buffer, &pos, FS_MAX_FILESIZE, "\n");
    }
    buffer[pos] = '\0';
    *size = pos;
    return 0;
}

static int proc_list(fs_file_info_t** files, int* count) {
    memset(proc_entries, 0, sizeof(proc_entries));
    strcpy(proc_entries[0].name, "processes");
    proc_entries[0].used = 1;
    proc_entries[0].size = 0;
    strcpy(proc_entries[1].name, "net");
    proc_entries[1].used = 1;
    proc_entries[1].is_directory = 1;
    *files = proc_entries;
    *count = 2;
    return 0;
}

static int readonly_fail(const char* path, const char* data, uint32_t size) {
    (void)path; (void)data; (void)size; return -1;
}

static int path_fail(const char* path) { (void)path; return -1; }
static int chdir_fail(const char* path) { (void)path; return -1; }
static const char* proc_cwd(void) { return "/proc"; }
static const char* dev_cwd(void) { return "/dev"; }

static const vfs_ops_t proc_ops = {
    "procfs",
    readonly_fail, proc_read, path_fail, proc_list,
    path_fail, path_fail, chdir_fail, proc_cwd, ramfs_disk_info
};

static int dev_read(const char* path, char* buffer, uint32_t* size) {
    if (!path) return -1;
    if (strcmp(path, "null") == 0) { *size = 0; return 0; }
    if (strcmp(path, "zero") == 0) {
        memset(buffer, 0, 512);
        *size = 512;
        return 0;
    }
    return -1;
}

static int dev_list(fs_file_info_t** files, int* count) {
    memset(dev_entries, 0, sizeof(dev_entries));
    strcpy(dev_entries[0].name, "null"); dev_entries[0].used = 1;
    strcpy(dev_entries[1].name, "zero"); dev_entries[1].used = 1;
    *files = dev_entries;
    *count = 2;
    return 0;
}

static const vfs_ops_t dev_ops = {
    "devfs",
    readonly_fail, dev_read, path_fail, dev_list,
    path_fail, path_fail, chdir_fail, dev_cwd, ramfs_disk_info
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
        vfs_mount("/dev", &dev_ops);
        vfs_mount("/proc", &proc_ops);
        klog_info("VFS mounted FAT root filesystem");
    } else {
        ramfs_init();
        vfs_mount_root(&ramfs_ops);
        vfs_mount("/dev", &dev_ops);
        vfs_mount("/proc", &proc_ops);
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

int fs_mount(const char* type, const char* path) {
    if (!type || !path) return -1;
    if (strcmp(type, "proc") == 0 || strcmp(type, "procfs") == 0) return vfs_mount(path, &proc_ops);
    if (strcmp(type, "dev") == 0 || strcmp(type, "devfs") == 0) return vfs_mount(path, &dev_ops);
    if (strcmp(type, "ramfs") == 0) return vfs_mount(path, &ramfs_ops);
    if (strcmp(type, "root") == 0) return vfs_mount(path, fat_available() ? &fat_ops : &ramfs_ops);
    return -1;
}

int fs_umount(const char* path) {
    return vfs_umount(path);
}

int fs_mounts(char* buffer, uint32_t* size, uint32_t max_size) {
    return vfs_get_mounts(buffer, size, max_size);
}
