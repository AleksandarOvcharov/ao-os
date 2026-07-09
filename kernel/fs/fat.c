#include "fat.h"
#include "fs.h"
#include "ata.h"
#include "block_cache.h"
#include "string.h"
#include "klog.h"

#define ata_read_sector block_cache_read_sector
#define ata_write_sector block_cache_write_sector

#define SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define ATTR_DIRECTORY 0x10
#define ATTR_VOLUME_ID 0x08
#define ATTR_LFN 0x0F
#define MAX_DIR_ENTRIES 512

typedef struct {
    uint8_t jump[3]; char oem[8]; uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster; uint16_t reserved_sectors;
    uint8_t fat_count; uint16_t root_entries; uint16_t total16;
    uint8_t media; uint16_t fat16_size; uint16_t sectors_per_track;
    uint16_t heads; uint32_t hidden; uint32_t total32;
} __attribute__((packed)) fat_bpb_t;

typedef struct {
    char name[8]; char ext[3]; uint8_t attr; uint8_t nt_reserved;
    uint8_t create_tenths; uint16_t create_time, create_date, access_date;
    uint16_t cluster_high; uint16_t modify_time, modify_date;
    uint16_t cluster_low; uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

static fat_bpb_t bpb;
static fat_type_t type;
static uint32_t total_sectors, fat_size, root_sectors, first_data;
static uint32_t cluster_count, root_cluster, current_cluster;
static uint16_t fsinfo_sector, backup_boot_sector;
static uint8_t mounted_drive;
static char cwd[FS_MAX_FILENAME] = "/";
static fs_file_info_t file_list[MAX_DIR_ENTRIES];

static uint32_t entry_cluster(const fat_dir_entry_t* e) {
    uint32_t c = e->cluster_low;
    if (type == FAT_TYPE_32) c |= (uint32_t)e->cluster_high << 16;
    return c;
}

static void set_entry_cluster(fat_dir_entry_t* e, uint32_t c) {
    e->cluster_low = (uint16_t)c;
    e->cluster_high = type == FAT_TYPE_32 ? (uint16_t)(c >> 16) : 0;
}

static uint32_t eoc_value(void) {
    return type == FAT_TYPE_12 ? 0xFFF : (type == FAT_TYPE_16 ? 0xFFFF : 0x0FFFFFFF);
}

static int is_eoc(uint32_t c) {
    return c >= (type == FAT_TYPE_12 ? 0xFF8 : (type == FAT_TYPE_16 ? 0xFFF8 : 0x0FFFFFF8));
}

static int valid_data_cluster(uint32_t c) {
    return c >= 2 && c < cluster_count + 2;
}

static uint32_t fat_get(uint32_t cluster);

/* Advance a corruption-check cursor. EOC is a normal terminal state; zero,
   reserved/bad values and references outside the data area are invalid. */
static int checked_next(uint32_t c, uint32_t* next, int* terminal) {
    uint32_t value;
    if (!valid_data_cluster(c)) return -1;
    value = fat_get(c);
    if (is_eoc(value)) { *terminal = 1; *next = value; return 0; }
    if (!valid_data_cluster(value)) return -1;
    *terminal = 0; *next = value; return 0;
}

static uint32_t cluster_sector(uint32_t c) {
    return first_data + (c - 2) * bpb.sectors_per_cluster;
}

static uint32_t fat_get(uint32_t cluster) {
    uint8_t sec[SECTOR_SIZE];
    uint32_t offset = type == FAT_TYPE_12 ? cluster + cluster / 2 :
                      (type == FAT_TYPE_16 ? cluster * 2 : cluster * 4);
    uint32_t sector = bpb.reserved_sectors + offset / SECTOR_SIZE;
    uint32_t pos = offset % SECTOR_SIZE;
    if (ata_read_sector(sector, sec) != 0) return eoc_value();
    if (type == FAT_TYPE_12) {
        uint8_t next;
        uint16_t v;
        if (pos == 511) {
            if (ata_read_sector(sector + 1, &next) != 0) return eoc_value();
            v = sec[pos] | ((uint16_t)next << 8);
        } else v = sec[pos] | ((uint16_t)sec[pos + 1] << 8);
        return cluster & 1 ? v >> 4 : v & 0xFFF;
    }
    if (type == FAT_TYPE_16) return sec[pos] | ((uint16_t)sec[pos + 1] << 8);
    return (*(uint32_t*)&sec[pos]) & 0x0FFFFFFF;
}

static int fat_set_copy(uint32_t base, uint32_t cluster, uint32_t value) {
    uint8_t sec[SECTOR_SIZE], next[SECTOR_SIZE];
    uint32_t offset = type == FAT_TYPE_12 ? cluster + cluster / 2 :
                      (type == FAT_TYPE_16 ? cluster * 2 : cluster * 4);
    uint32_t sector = base + offset / SECTOR_SIZE, pos = offset % SECTOR_SIZE;
    if (ata_read_sector(sector, sec) != 0) return -1;
    if (type == FAT_TYPE_12) {
        uint16_t old;
        if (pos == 511) {
            if (ata_read_sector(sector + 1, next) != 0) return -1;
            old = sec[pos] | ((uint16_t)next[0] << 8);
            old = cluster & 1 ? (old & 0x000F) | (value << 4) : (old & 0xF000) | (value & 0xFFF);
            sec[pos] = (uint8_t)old; next[0] = (uint8_t)(old >> 8);
            if (ata_write_sector(sector + 1, next) != 0) return -1;
        } else {
            old = sec[pos] | ((uint16_t)sec[pos + 1] << 8);
            old = cluster & 1 ? (old & 0x000F) | (value << 4) : (old & 0xF000) | (value & 0xFFF);
            sec[pos] = (uint8_t)old; sec[pos + 1] = (uint8_t)(old >> 8);
        }
    } else if (type == FAT_TYPE_16) {
        sec[pos] = (uint8_t)value; sec[pos + 1] = (uint8_t)(value >> 8);
    } else {
        uint32_t old = *(uint32_t*)&sec[pos];
        *(uint32_t*)&sec[pos] = (old & 0xF0000000) | (value & 0x0FFFFFFF);
    }
    return ata_write_sector(sector, sec);
}

static int fat_set(uint32_t cluster, uint32_t value) {
    for (uint32_t i = 0; i < bpb.fat_count; i++)
        if (fat_set_copy(bpb.reserved_sectors + i * fat_size, cluster, value) != 0) return -1;
    return 0;
}

static void fsinfo_adjust_sector(uint32_t sector, int delta, uint32_t next_free) {
    uint8_t data[SECTOR_SIZE];
    uint32_t free_count;
    if (type != FAT_TYPE_32 || sector >= bpb.reserved_sectors || ata_read_sector(sector, data) != 0) return;
    if (*(uint32_t*)&data[0] != 0x41615252 || *(uint32_t*)&data[484] != 0x61417272 ||
        *(uint32_t*)&data[508] != 0xAA550000) return;
    free_count = *(uint32_t*)&data[488];
    if (free_count != 0xFFFFFFFF) *(uint32_t*)&data[488] = free_count + delta;
    *(uint32_t*)&data[492] = next_free < cluster_count + 2 ? next_free : 2;
    ata_write_sector(sector, data);
}

static void fsinfo_adjust(int delta, uint32_t next_free) {
    if (type != FAT_TYPE_32 || fsinfo_sector == 0 || fsinfo_sector == 0xFFFF) return;
    fsinfo_adjust_sector(fsinfo_sector, delta, next_free);
    if (backup_boot_sector != 0 && backup_boot_sector != 0xFFFF)
        fsinfo_adjust_sector((uint32_t)backup_boot_sector + 1, delta, next_free);
}

static uint32_t alloc_cluster(void) {
    uint8_t zero[SECTOR_SIZE]; memset(zero, 0, sizeof(zero));
    for (uint32_t c = 2; c < cluster_count + 2; c++) {
        if (fat_get(c) == 0) {
            if (fat_set(c, eoc_value()) != 0) return 0;
            for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++)
                if (ata_write_sector(cluster_sector(c) + s, zero) != 0) return 0;
            fsinfo_adjust(-1, c + 1);
            return c;
        }
    }
    return 0;
}

static void free_chain(uint32_t c) {
    while (c >= 2 && c < cluster_count + 2) {
        uint32_t next = fat_get(c);
        fat_set(c, 0);
        fsinfo_adjust(1, c);
        if (is_eoc(next) || next < 2) break;
        c = next;
    }
}

/* Resolve a directory entry index to its sector and byte offset. */
static int dir_location(uint32_t dir, int index, uint32_t* sector, uint32_t* offset) {
    if (dir == 0 && type != FAT_TYPE_32) {
        if ((uint32_t)index >= bpb.root_entries) return -1;
        *sector = bpb.reserved_sectors + bpb.fat_count * fat_size + (uint32_t)index / 16;
        *offset = ((uint32_t)index % 16) * DIR_ENTRY_SIZE;
        return 0;
    }
    uint32_t c = dir ? dir : root_cluster;
    uint32_t per_cluster = bpb.sectors_per_cluster * 16;
    uint32_t skip = (uint32_t)index / per_cluster;
    while (skip--) {
        c = fat_get(c);
        if (is_eoc(c) || c < 2) return -1;
    }
    uint32_t within = (uint32_t)index % per_cluster;
    *sector = cluster_sector(c) + within / 16;
    *offset = (within % 16) * DIR_ENTRY_SIZE;
    return 0;
}

static int dir_read(uint32_t dir, int index, fat_dir_entry_t* e) {
    uint8_t sec[SECTOR_SIZE]; uint32_t sector, offset;
    if (dir_location(dir, index, &sector, &offset) != 0 || ata_read_sector(sector, sec) != 0) return -1;
    memcpy(e, sec + offset, sizeof(*e)); return 0;
}

static int dir_write(uint32_t dir, int index, const fat_dir_entry_t* e) {
    uint8_t sec[SECTOR_SIZE]; uint32_t sector, offset;
    if (dir_location(dir, index, &sector, &offset) != 0 || ata_read_sector(sector, sec) != 0) return -1;
    memcpy(sec + offset, e, sizeof(*e)); return ata_write_sector(sector, sec);
}

static void make_sfn(const char* input, char out[11]) {
    memset(out, ' ', 11); int i = 0, p = 0;
    while (input[p] && input[p] != '.' && i < 8) {
        char c = input[p++]; out[i++] = c >= 'a' && c <= 'z' ? c - 32 : c;
    }
    while (input[p] && input[p] != '.') p++;
    if (input[p] == '.') p++;
    i = 8;
    while (input[p] && i < 11) {
        char c = input[p++]; out[i++] = c >= 'a' && c <= 'z' ? c - 32 : c;
    }
}

static int name_match(const fat_dir_entry_t* e, const char* name) {
    char sfn[11]; make_sfn(name, sfn);
    for (int i = 0; i < 8; i++) if (e->name[i] != sfn[i]) return 0;
    for (int i = 0; i < 3; i++) if (e->ext[i] != sfn[8 + i]) return 0;
    return 1;
}

static int find_entry(uint32_t dir, const char* name, fat_dir_entry_t* out, int* out_index) {
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        fat_dir_entry_t e; if (dir_read(dir, i, &e) != 0) break;
        if ((uint8_t)e.name[0] == 0) break;
        if ((uint8_t)e.name[0] == 0xE5 || e.attr == ATTR_LFN || (e.attr & ATTR_VOLUME_ID)) continue;
        if (name_match(&e, name)) { if (out) *out = e; if (out_index) *out_index = i; return 0; }
    }
    return -1;
}

static int free_entry_index(uint32_t dir) {
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        fat_dir_entry_t e;
        if (dir_read(dir, i, &e) != 0) {
            /* FAT32 roots and all subdirectories are cluster chains. */
            if (dir == 0 && type != FAT_TYPE_32) return -1;
            uint32_t c = dir ? dir : root_cluster;
            while (1) { uint32_t next = fat_get(c); if (is_eoc(next)) break; if (next < 2) return -1; c = next; }
            uint32_t added = alloc_cluster();
            if (!added || fat_set(c, added) != 0) return -1;
            return i;
        }
        if ((uint8_t)e.name[0] == 0 || (uint8_t)e.name[0] == 0xE5) return i;
    }
    return -1;
}

static int mount_drive(int drive) {
    uint8_t sector[SECTOR_SIZE]; ata_select_drive(drive);
    if (ata_read_sector(0, sector) != 0 || sector[510] != 0x55 || sector[511] != 0xAA) return -1;
    memcpy(&bpb, sector, sizeof(bpb));
    if (bpb.bytes_per_sector != 512 || !bpb.sectors_per_cluster || !bpb.fat_count || !bpb.reserved_sectors) return -1;
    total_sectors = bpb.total16 ? bpb.total16 : bpb.total32;
    fat_size = bpb.fat16_size ? bpb.fat16_size : *(uint32_t*)&sector[36];
    root_sectors = ((uint32_t)bpb.root_entries * 32 + 511) / 512;
    first_data = bpb.reserved_sectors + bpb.fat_count * fat_size + root_sectors;
    if (!fat_size || total_sectors <= first_data) return -1;
    cluster_count = (total_sectors - first_data) / bpb.sectors_per_cluster;
    type = cluster_count < 4085 ? FAT_TYPE_12 : (cluster_count < 65525 ? FAT_TYPE_16 : FAT_TYPE_32);
    root_cluster = type == FAT_TYPE_32 ? (*(uint32_t*)&sector[44] & 0x0FFFFFFF) : 0;
    fsinfo_sector = type == FAT_TYPE_32 ? *(uint16_t*)&sector[48] : 0;
    backup_boot_sector = type == FAT_TYPE_32 ? *(uint16_t*)&sector[50] : 0;
    if (type == FAT_TYPE_32 && root_cluster < 2) return -1;
    current_cluster = type == FAT_TYPE_32 ? root_cluster : 0;
    mounted_drive = (uint8_t)drive; cwd[0] = '/'; cwd[1] = 0; return 0;
}

void fat_init(void) {
    type = FAT_TYPE_NONE;
    if (!ata_is_available()) return;
    if (mount_drive(0) != 0 && mount_drive(1) != 0) { ata_select_drive(0); type = FAT_TYPE_NONE; return; }
    if (type == FAT_TYPE_12) klog_info("FAT12 filesystem initialized");
    else if (type == FAT_TYPE_16) klog_info("FAT16 filesystem initialized");
    else klog_info("FAT32 filesystem initialized");
}
int fat_available(void) { return type != FAT_TYPE_NONE; }
fat_type_t fat_get_type(void) { return type; }

int fat_read(const char* name, char* buffer, uint32_t* size) {
    fat_dir_entry_t e; if (!fat_available() || find_entry(current_cluster, name, &e, 0) || (e.attr & ATTR_DIRECTORY)) return -1;
    uint32_t c = entry_cluster(&e), done = 0;
    uint32_t cycle_anchor = c, cycle_power = 1, cycle_length = 0;
    while (done < e.size) {
        if (!valid_data_cluster(c)) { *size = done; return -1; }
        for (uint32_t s = 0; s < bpb.sectors_per_cluster && done < e.size; s++) {
            uint8_t sec[SECTOR_SIZE]; if (ata_read_sector(cluster_sector(c) + s, sec) != 0) return -1;
            uint32_t n = e.size - done; if (n > SECTOR_SIZE) n = SECTOR_SIZE;
            memcpy(buffer + done, sec, n); done += n;
        }
        if (done < e.size) {
            uint32_t next;
            int terminal;
            if (checked_next(c, &next, &terminal) != 0 || terminal) { *size = done; return -1; }
            c = next;
            cycle_length++;
            if (c == cycle_anchor) { *size = done; return -1; }
            if (cycle_length == cycle_power) {
                cycle_anchor = c;
                cycle_power <<= 1;
                cycle_length = 0;
            }
        }
    }
    *size = done; return done == e.size ? 0 : -1;
}

int fat_create(const char* name, const char* data, uint32_t size) {
    if (!fat_available() || !name || !*name) return -1;
    fat_dir_entry_t e; int index;
    if (find_entry(current_cluster, name, &e, &index) == 0) {
        if (e.attr & ATTR_DIRECTORY) return -1;
        free_chain(entry_cluster(&e));
    } else {
        index = free_entry_index(current_cluster); if (index < 0) return -1;
        memset(&e, 0, sizeof(e)); char sfn[11]; make_sfn(name, sfn);
        memcpy(e.name, sfn, 8); memcpy(e.ext, sfn + 8, 3); e.attr = 0x20;
    }
    uint32_t first = 0, prev = 0, written = 0;
    while (written < size || (!size && !first)) {
        uint32_t c = alloc_cluster(); if (!c) { free_chain(first); return -1; }
        if (!first) first = c;
        if (prev && fat_set(prev, c) != 0) { free_chain(first); return -1; }
        for (uint32_t s = 0; s < bpb.sectors_per_cluster && written < size; s++) {
            uint8_t sec[SECTOR_SIZE]; memset(sec, 0, sizeof(sec));
            uint32_t n = size - written; if (n > SECTOR_SIZE) n = SECTOR_SIZE;
            memcpy(sec, data + written, n); if (ata_write_sector(cluster_sector(c) + s, sec) != 0) { free_chain(first); return -1; }
            written += n;
        }
        prev = c;
    }
    set_entry_cluster(&e, first); e.size = size; return dir_write(current_cluster, index, &e);
}

int fat_delete(const char* name) {
    fat_dir_entry_t e; int i;
    if (find_entry(current_cluster, name, &e, &i) || (e.attr & ATTR_DIRECTORY)) return -1;
    free_chain(entry_cluster(&e)); e.name[0] = (char)0xE5; return dir_write(current_cluster, i, &e);
}

int fat_list(void** entries, int* count) {
    *count = 0;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        fat_dir_entry_t e; if (dir_read(current_cluster, i, &e) != 0 || (uint8_t)e.name[0] == 0) break;
        if ((uint8_t)e.name[0] == 0xE5 || e.attr == ATTR_LFN || (e.attr & ATTR_VOLUME_ID)) continue;
        fs_file_info_t* f = &file_list[(*count)++]; memset(f, 0, sizeof(*f)); int p = 0;
        for (int j = 0; j < 8 && e.name[j] != ' '; j++) f->name[p++] = e.name[j];
        if (!(e.attr & ATTR_DIRECTORY) && e.ext[0] != ' ') { f->name[p++] = '.'; for (int j = 0; j < 3 && e.ext[j] != ' '; j++) f->name[p++] = e.ext[j]; }
        f->size = e.size; f->used = 1; f->is_directory = !!(e.attr & ATTR_DIRECTORY);
    }
    *entries = file_list; return 0;
}

int fat_mkdir(const char* name) {
    fat_dir_entry_t e; if (!name || !*name || find_entry(current_cluster, name, 0, 0) == 0) return -1;
    int index = free_entry_index(current_cluster); uint32_t c = alloc_cluster(); if (index < 0 || !c) return -1;
    memset(&e, 0, sizeof(e)); char sfn[11]; make_sfn(name, sfn); memcpy(e.name, sfn, 8); memcpy(e.ext, sfn + 8, 3); e.attr = ATTR_DIRECTORY; set_entry_cluster(&e, c);
    fat_dir_entry_t dot; memset(&dot, 0, sizeof(dot)); memset(dot.name, ' ', 8); memset(dot.ext, ' ', 3); dot.name[0] = '.'; dot.attr = ATTR_DIRECTORY; set_entry_cluster(&dot, c); dir_write(c, 0, &dot);
    dot.name[1] = '.'; set_entry_cluster(&dot, current_cluster == root_cluster ? 0 : current_cluster); dir_write(c, 1, &dot);
    return dir_write(current_cluster, index, &e);
}

int fat_rmdir(const char* name) {
    fat_dir_entry_t e; int i; if (find_entry(current_cluster, name, &e, &i) || !(e.attr & ATTR_DIRECTORY)) return -1;
    uint32_t c = entry_cluster(&e);
    for (int j = 2; j < MAX_DIR_ENTRIES; j++) { fat_dir_entry_t x; if (dir_read(c, j, &x) != 0 || x.name[0] == 0) break; if ((uint8_t)x.name[0] != 0xE5) return -1; }
    free_chain(c); e.name[0] = (char)0xE5; return dir_write(current_cluster, i, &e);
}

int fat_chdir(const char* name) {
    if (!name || !*name || strcmp(name, "/") == 0) { current_cluster = type == FAT_TYPE_32 ? root_cluster : 0; cwd[0]='/'; cwd[1]=0; return 0; }
    if (strcmp(name, "..") == 0) {
        if ((type != FAT_TYPE_32 && current_cluster == 0) || (type == FAT_TYPE_32 && current_cluster == root_cluster)) return 0;
        fat_dir_entry_t e; if (dir_read(current_cluster, 1, &e) != 0) return -1; uint32_t p = entry_cluster(&e); current_cluster = p ? p : (type == FAT_TYPE_32 ? root_cluster : 0);
        int n = strlen(cwd); while (n > 1 && cwd[n-1] == '/') cwd[--n]=0; while (n > 1 && cwd[n-1] != '/') cwd[--n]=0; return 0;
    }
    fat_dir_entry_t e; if (find_entry(current_cluster, name, &e, 0) || !(e.attr & ATTR_DIRECTORY)) return -1;
    current_cluster = entry_cluster(&e); int n = strlen(cwd); if (n > 1 && cwd[n-1] != '/') cwd[n++]='/'; for (int i=0; name[i] && n<FS_MAX_FILENAME-1; i++) cwd[n++]=name[i]; cwd[n]=0; return 0;
}
const char* fat_getcwd(void) { return cwd; }

void fat_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    uint32_t free = 0; for (uint32_t c=2; c<cluster_count+2; c++) if (fat_get(c)==0) free++;
    uint32_t bytes = bpb.sectors_per_cluster * 512; *total_kb = cluster_count * bytes / 1024; *free_kb = free * bytes / 1024; *used_kb = *total_kb - *free_kb;
    (void)mounted_drive;
}
