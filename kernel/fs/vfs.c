#include "vfs.h"
#include "string.h"

#define VFS_MAX_MOUNTS 8

typedef struct {
    char path[32];
    const vfs_ops_t* ops;
} mount_t;

static mount_t mounts[VFS_MAX_MOUNTS];
static int mount_count;

static int is_sep(char c) {
    return c == '/' || c == '\0';
}

void vfs_init(void) {
    mount_count = 0;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        mounts[i].path[0] = '\0';
        mounts[i].ops = 0;
    }
}

int vfs_mount(const char* path, const vfs_ops_t* ops) {
    if (!path || !ops || path[0] != '/') return -1;

    int len = (int)strlen(path);
    if (len <= 0 || len >= (int)sizeof(mounts[0].path)) return -1;
    while (len > 1 && path[len - 1] == '/') len--;

    for (int i = 0; i < mount_count; i++) {
        if (strncmp(mounts[i].path, path, (size_t)len) == 0 && mounts[i].path[len] == '\0') {
            mounts[i].ops = ops;
            return 0;
        }
    }

    if (mount_count >= VFS_MAX_MOUNTS) return -1;
    strncpy(mounts[mount_count].path, path, (size_t)len);
    mounts[mount_count].path[len] = '\0';
    mounts[mount_count].ops = ops;
    mount_count++;
    return 0;
}

int vfs_mount_root(const vfs_ops_t* ops) {
    return vfs_mount("/", ops);
}

int vfs_umount(const char* path) {
    if (!path || path[0] != '/' || strcmp(path, "/") == 0) return -1;
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mounts[i].path, path) == 0) {
            for (int j = i; j < mount_count - 1; j++) mounts[j] = mounts[j + 1];
            mount_count--;
            mounts[mount_count].path[0] = '\0';
            mounts[mount_count].ops = 0;
            return 0;
        }
    }
    return -1;
}

static void append(char* buffer, uint32_t* pos, uint32_t max, const char* s) {
    while (*s && *pos + 1 < max) buffer[(*pos)++] = *s++;
}

int vfs_get_mounts(char* buffer, uint32_t* size, uint32_t max_size) {
    if (!buffer || !size || max_size == 0) return -1;
    uint32_t pos = 0;
    for (int i = 0; i < mount_count; i++) {
        append(buffer, &pos, max_size, mounts[i].path);
        append(buffer, &pos, max_size, " ");
        append(buffer, &pos, max_size, mounts[i].ops ? mounts[i].ops->name : "none");
        append(buffer, &pos, max_size, "\n");
    }
    buffer[pos < max_size ? pos : max_size - 1] = '\0';
    *size = pos;
    return 0;
}

static const vfs_ops_t* resolve(const char* path, const char** subpath) {
    if (mount_count == 0) return 0;
    if (!path || path[0] == '\0' || path[0] != '/') {
        *subpath = path;
        return mounts[0].ops;
    }

    int best = -1;
    int best_len = -1;
    for (int i = 0; i < mount_count; i++) {
        int len = (int)strlen(mounts[i].path);
        if (len == 1) {
            if (best_len < 1) { best = i; best_len = 1; }
            continue;
        }
        if (strncmp(path, mounts[i].path, (size_t)len) == 0 && is_sep(path[len]) && len > best_len) {
            best = i;
            best_len = len;
        }
    }

    if (best < 0) return 0;
    if (best_len == 1) {
        *subpath = path[1] ? path + 1 : "";
    } else {
        *subpath = path[best_len] == '/' ? path + best_len + 1 : "";
    }
    return mounts[best].ops;
}

const char* vfs_get_root_type(void) {
    return mount_count > 0 && mounts[0].ops ? mounts[0].ops->name : "none";
}

int vfs_create(const char* path, const char* data, uint32_t size) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->create ? ops->create(sub, data, size) : -1;
}

int vfs_read(const char* path, char* buffer, uint32_t* size) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->read ? ops->read(sub, buffer, size) : -1;
}

int vfs_delete(const char* path) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->delete ? ops->delete(sub) : -1;
}

int vfs_list(fs_file_info_t** files, int* count) {
    const char* sub; const vfs_ops_t* ops = resolve("", &sub);
    return ops && ops->list ? ops->list(files, count) : -1;
}

int vfs_mkdir(const char* path) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->mkdir ? ops->mkdir(sub) : -1;
}

int vfs_rmdir(const char* path) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->rmdir ? ops->rmdir(sub) : -1;
}

int vfs_chdir(const char* path) {
    const char* sub; const vfs_ops_t* ops = resolve(path, &sub);
    return ops && ops->chdir ? ops->chdir(sub) : -1;
}

const char* vfs_getcwd(void) {
    return mount_count > 0 && mounts[0].ops && mounts[0].ops->getcwd ? mounts[0].ops->getcwd() : "/";
}

void vfs_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    if (mount_count > 0 && mounts[0].ops && mounts[0].ops->disk_info) {
        mounts[0].ops->disk_info(total_kb, used_kb, free_kb);
    } else {
        *total_kb = 0;
        *used_kb = 0;
        *free_kb = 0;
    }
}
