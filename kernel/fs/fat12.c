#include "fat12.h"
#include "fs.h"
#include "ata.h"
#include "string.h"
#include "klog.h"
#include "vga.h"

#define FAT12_ATTR_LFN 0x0F

// LFN entry byte offsets (raw access to avoid packed member warnings)
// order:1, name1:5 chars at offset 1, attr at 11, type at 12, checksum at 13
// name2:6 chars at offset 14, first_cluster at 26, name3:2 chars at offset 28

// Read one UTF-16LE char from a packed LFN field (2 bytes at byte offset)
static uint16_t fat12_lfn_char(const uint8_t* raw, int idx) {
    return (uint16_t)raw[idx * 2] | ((uint16_t)raw[idx * 2 + 1] << 8);
}

// Extract LFN from preceding LFN entries into buf (ASCII only, max 255 chars)
static void fat12_read_lfn(fat12_dir_entry_t* entries, int sfn_idx, char* buf, int buf_size) {
    char tmp[256];
    int tmp_len = 0;
    int seq[20];
    int seq_count = 0;
    for (int k = sfn_idx - 1; k >= 0 && seq_count < 20; k--) {
        uint8_t attr = (uint8_t)entries[k].attributes;
        if (attr != FAT12_ATTR_LFN) break;
        const uint8_t* raw = (const uint8_t*)&entries[k];
        uint8_t order = raw[0];
        seq[seq_count++] = k;
        if (order & 0x40) break;
    }
    // Reverse to get correct order (seq[seq_count-1] is first chunk)
    for (int k = seq_count - 1; k >= 0 && tmp_len < buf_size - 1; k--) {
        const uint8_t* raw = (const uint8_t*)&entries[seq[k]];
        // name1: bytes 1-10 (5 chars), name2: bytes 14-25 (6 chars), name3: bytes 28-31 (2 chars)
        int offsets[3] = {1, 14, 28};
        int lens[3]    = {5,  6,  2};
        for (int p = 0; p < 3 && tmp_len < buf_size - 1; p++) {
            for (int c = 0; c < lens[p] && tmp_len < buf_size - 1; c++) {
                uint16_t ch = fat12_lfn_char(raw + offsets[p], c);
                if (ch == 0x0000 || ch == 0xFFFF) goto done;
                tmp[tmp_len++] = (char)(ch & 0x7F);
            }
        }
    }
done:
    tmp[tmp_len] = '\0';
    if (tmp_len >= buf_size) tmp_len = buf_size - 1;
    for (int k = 0; k < tmp_len; k++) buf[k] = tmp[k];
    buf[tmp_len] = '\0';
}

static fat12_bpb_t bpb;
static uint8_t fat_table[FAT12_SECTOR_SIZE * 9];
static fat12_dir_entry_t root_dir[224];
static fat12_dir_entry_t current_dir_entries[224];
static int fat12_initialized = 0;
static char current_dir[256] = "/";
static uint16_t current_dir_cluster = 0; // 0 = root directory

static uint16_t fat12_get_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2);
    if (fat_offset + 1 >= sizeof(fat_table)) return 0xFFF;
    uint16_t fat_value = *(uint16_t*)&fat_table[fat_offset];
    
    if (cluster & 1) {
        fat_value >>= 4;
    } else {
        fat_value &= 0x0FFF;
    }
    
    return fat_value;
}

static void fat12_set_cluster_value(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster + (cluster / 2);
    if (fat_offset + 1 >= sizeof(fat_table)) return;
    uint16_t* fat_entry = (uint16_t*)&fat_table[fat_offset];
    
    if (cluster & 1) {
        *fat_entry = (*fat_entry & 0x000F) | (value << 4);
    } else {
        *fat_entry = (*fat_entry & 0xF000) | (value & 0x0FFF);
    }
}

static uint16_t fat12_find_free_cluster(void) {
    for (uint16_t cluster = 2; cluster < 2849; cluster++) {
        uint16_t value = fat12_get_next_cluster(cluster);
        if (value == 0x000) {
            return cluster;
        }
    }
    return 0;
}

void fat12_get_disk_info(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    if (!fat12_initialized) {
        *total_kb = *used_kb = *free_kb = 0;
        return;
    }

    uint32_t total_sectors = bpb.total_sectors ? bpb.total_sectors : bpb.large_sector_count;
    uint32_t bytes_per_cluster = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint32_t data_start = bpb.reserved_sectors +
                          (bpb.fat_count * bpb.sectors_per_fat) +
                          ((bpb.root_dir_entries * 32 + 511) / 512);
    uint32_t data_sectors = (total_sectors > data_start) ? (total_sectors - data_start) : 0;
    uint32_t total_clusters = data_sectors / bpb.sectors_per_cluster;

    uint32_t free_clusters = 0;
    for (uint16_t c = 2; c < (uint16_t)(total_clusters + 2); c++) {
        if (fat12_get_next_cluster(c) == 0x000) free_clusters++;
    }

    uint32_t used_clusters = total_clusters - free_clusters;
    *total_kb = (total_clusters * bytes_per_cluster) / 1024;
    *used_kb  = (used_clusters  * bytes_per_cluster) / 1024;
    *free_kb  = (free_clusters  * bytes_per_cluster) / 1024;
}

// Removed - now search current_dir_entries inline in each function

static void fat12_write_fat_table(void) {
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_size = bpb.sectors_per_fat;
    
    // Write both FAT copies
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_write_sector(fat_start + i, &fat_table[i * 512]);
    }
    
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_write_sector(fat_start + fat_size + i, &fat_table[i * 512]);
    }
    
    // Reload FAT to ensure consistency
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_read_sector(fat_start + i, &fat_table[i * 512]);
    }
}

static void fat12_write_root_dir(void) {
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_size = bpb.sectors_per_fat;
    uint32_t root_dir_start = fat_start + (bpb.fat_count * fat_size);
    uint32_t root_dir_sectors = (bpb.root_dir_entries * 32 + 511) / 512;
    
    for (uint32_t i = 0; i < root_dir_sectors && i < 14; i++) {
        ata_write_sector(root_dir_start + i, (uint8_t*)&root_dir[i * 16]);
    }
}

static void fat12_reload_root_dir(void) {
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_size = bpb.sectors_per_fat;
    uint32_t root_dir_start = fat_start + (bpb.fat_count * fat_size);
    uint32_t root_dir_sectors = (bpb.root_dir_entries * 32 + 511) / 512;
    
    for (uint32_t i = 0; i < root_dir_sectors && i < 14; i++) {
        ata_read_sector(root_dir_start + i, (uint8_t*)&root_dir[i * 16]);
    }
}

static void fat12_load_directory(uint16_t cluster, fat12_dir_entry_t* entries) {
    memset(entries, 0, sizeof(fat12_dir_entry_t) * 224);
    
    if (cluster == 0) {
        // Root directory
        memcpy(entries, root_dir, sizeof(fat12_dir_entry_t) * 224);
        return;
    }
    
    // Load from cluster
    uint32_t data_start = bpb.reserved_sectors + 
                         (bpb.fat_count * bpb.sectors_per_fat) +
                         ((bpb.root_dir_entries * 32 + 511) / 512);
    
    int entry_idx = 0;
    while (cluster >= 2 && cluster < 0xFF8 && entry_idx < 224) {
        uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        uint8_t sector_data[512];
        ata_read_sector(sector, sector_data);
        
        // Each sector holds 16 directory entries (512 / 32)
        for (int i = 0; i < 16 && entry_idx < 224; i++, entry_idx++) {
            memcpy(&entries[entry_idx], &sector_data[i * 32], 32);
        }
        
        cluster = fat12_get_next_cluster(cluster);
    }
}

static void fat12_write_directory(uint16_t cluster, fat12_dir_entry_t* entries) {
    if (cluster == 0) {
        // Root directory
        memcpy(root_dir, entries, sizeof(fat12_dir_entry_t) * 224);
        fat12_write_root_dir();
        return;
    }
    
    // Write to cluster
    uint32_t data_start = bpb.reserved_sectors + 
                         (bpb.fat_count * bpb.sectors_per_fat) +
                         ((bpb.root_dir_entries * 32 + 511) / 512);
    
    int entry_idx = 0;
    while (cluster >= 2 && cluster < 0xFF8 && entry_idx < 224) {
        uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        uint8_t sector_data[512];
        memset(sector_data, 0, 512);
        
        // Each sector holds 16 directory entries
        for (int i = 0; i < 16 && entry_idx < 224; i++, entry_idx++) {
            memcpy(&sector_data[i * 32], &entries[entry_idx], 32);
        }
        
        ata_write_sector(sector, sector_data);
        cluster = fat12_get_next_cluster(cluster);
    }
}

static void fat12_parse_filename(const char* input, char* output) {
    int i = 0, j = 0;
    
    for (i = 0; i < 8 && input[j] && input[j] != '.'; i++, j++) {
        output[i] = input[j];
    }
    while (i < 8) output[i++] = ' ';
    
    if (input[j] == '.') j++;
    
    for (i = 8; i < 11 && input[j]; i++, j++) {
        output[i] = input[j];
    }
    while (i < 11) output[i++] = ' ';
}

static int fat12_compare_filename(const char* fat_name, const char* search_name) {
    char formatted[11];
    fat12_parse_filename(search_name, formatted);
    
    for (int i = 0; i < 11; i++) {
        char a = formatted[i];
        char b = fat_name[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return 0;
    }
    return 1;
}

// Case-insensitive string compare for LFN
static int fat12_stricmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

// Check if entry i matches search_name via LFN or 8.3
static int fat12_entry_matches(fat12_dir_entry_t* entries, int i, const char* search_name) {
    // Try LFN first
    char lfn[256];
    lfn[0] = '\0';
    fat12_read_lfn(entries, i, lfn, 256);
    if (lfn[0] != '\0' && fat12_stricmp(lfn, search_name)) return 1;

    // Fall back to 8.3
    char fat_filename[12];
    memcpy(fat_filename, entries[i].filename, 8);
    memcpy(fat_filename + 8, entries[i].extension, 3);
    fat_filename[11] = '\0';
    return fat12_compare_filename(fat_filename, search_name);
}

void fat12_init(void) {
    klog_info("Initializing FAT12 filesystem...");
    
    if (!ata_is_available()) {
        klog_warn("ATA not available, FAT12 disabled");
        fat12_initialized = 0;
        return;
    }
    
    uint8_t boot_sector[512];
    if (ata_read_sector(0, boot_sector) != 0) {
        klog_error("Failed to read boot sector");
        fat12_initialized = 0;
        return;
    }
    
    memcpy(&bpb, boot_sector, sizeof(fat12_bpb_t));
    
    if (bpb.bytes_per_sector != 512) {
        // Master drive has no valid FAT12 BPB — try slave (floppy.img on ide1)
        ata_select_drive(1);
        if (ata_read_sector(0, boot_sector) != 0) {
            klog_warn("Slave drive read failed, using ramfs");
            fat12_initialized = 0;
            return;
        }
        memcpy(&bpb, boot_sector, sizeof(fat12_bpb_t));
        if (bpb.bytes_per_sector != 512) {
            klog_warn("No FAT12 volume found, using ramfs");
            fat12_initialized = 0;
            return;
        }
        klog_info("FAT12 found on slave drive");
    }
    
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_size = bpb.sectors_per_fat;
    
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_read_sector(fat_start + i, &fat_table[i * 512]);
    }
    
    uint32_t root_dir_start = fat_start + (bpb.fat_count * fat_size);
    uint32_t root_dir_sectors = (bpb.root_dir_entries * 32 + 511) / 512;
    
    for (uint32_t i = 0; i < root_dir_sectors && i < 14; i++) {
        ata_read_sector(root_dir_start + i, (uint8_t*)&root_dir[i * 16]);
    }
    
    fat12_initialized = 1;
    klog_info("FAT12 filesystem initialized");
}

int fat12_available(void) {
    return fat12_initialized;
}

int fat12_read(const char* name, char* buffer, uint32_t* size) {
    if (!fat12_initialized) return -1;
    
    // Load current directory
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        if (current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY) continue;
        if (current_dir_entries[i].attributes & FAT12_ATTR_VOLUME_ID) continue;
        if ((uint8_t)current_dir_entries[i].attributes == FAT12_ATTR_LFN) continue;
        
        if (fat12_entry_matches(current_dir_entries, i, name)) {
            uint16_t cluster = current_dir_entries[i].first_cluster_low;
            uint32_t file_size = current_dir_entries[i].file_size;
            
            if (cluster >= 2) {
                uint32_t data_start = bpb.reserved_sectors + 
                                     (bpb.fat_count * bpb.sectors_per_fat) +
                                     ((bpb.root_dir_entries * 32 + 511) / 512);
                
                uint32_t bytes_read = 0;
                
                // Follow cluster chain
                while (cluster >= 2 && cluster < 0xFF8 && bytes_read < file_size) {
                    uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
                    uint8_t sector_data[512];
                    ata_read_sector(sector, sector_data);
                    
                    uint32_t bytes_to_read = file_size - bytes_read;
                    if (bytes_to_read > 512) bytes_to_read = 512;
                    
                    memcpy(buffer + bytes_read, sector_data, bytes_to_read);
                    bytes_read += bytes_to_read;
                    
                    cluster = fat12_get_next_cluster(cluster);
                }
                
                *size = bytes_read;
                return 0;
            }
        }
    }
    
    return -1;
}

int fat12_create(const char* name, const char* data, uint32_t size) {
    if (!fat12_initialized) return -1;
    
    // Validate name
    if (!name || name[0] == '\0') {
        klog_error("Invalid file name");
        return -1;
    }
    
    // Load current directory entries
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    // Check if file already exists - if so, update it
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        if (current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, current_dir_entries[i].filename, 8);
        memcpy(fat_filename + 8, current_dir_entries[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            // File exists - update it
            uint16_t old_cluster = current_dir_entries[i].first_cluster_low;
            
            // Free old cluster chain
            uint16_t cluster = old_cluster;
            while (cluster >= 2 && cluster < 0xFF8) {
                uint16_t next = fat12_get_next_cluster(cluster);
                fat12_set_cluster_value(cluster, 0x000);
                cluster = next;
            }
            
            // Allocate new clusters for updated content
            uint32_t clusters_needed = (size + 511) / 512;
            if (clusters_needed == 0) clusters_needed = 1;
            
            uint16_t first_cluster = 0;
            uint16_t prev_cluster = 0;
            
            for (uint32_t j = 0; j < clusters_needed; j++) {
                uint16_t free_cluster = fat12_find_free_cluster();
                if (free_cluster == 0) {
                    klog_error("No free clusters available");
                    return -1;
                }
                
                if (j == 0) {
                    first_cluster = free_cluster;
                } else {
                    fat12_set_cluster_value(prev_cluster, free_cluster);
                }
                
                prev_cluster = free_cluster;
            }
            
            fat12_set_cluster_value(prev_cluster, 0xFFF);
            
            // Update existing entry
            current_dir_entries[i].first_cluster_low = first_cluster;
            current_dir_entries[i].file_size = size;
            
            // Write data
            uint32_t data_start = bpb.reserved_sectors + 
                                 (bpb.fat_count * bpb.sectors_per_fat) +
                                 ((bpb.root_dir_entries * 32 + 511) / 512);
            
            cluster = first_cluster;
            uint32_t bytes_written = 0;
            
            while (cluster >= 2 && cluster < 0xFF8 && bytes_written < size) {
                uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
                uint8_t sector_data[512];
                memset(sector_data, 0, 512);
                
                uint32_t bytes_to_write = size - bytes_written;
                if (bytes_to_write > 512) bytes_to_write = 512;
                
                memcpy(sector_data, data + bytes_written, bytes_to_write);
                ata_write_sector(sector, sector_data);
                
                bytes_written += bytes_to_write;
                cluster = fat12_get_next_cluster(cluster);
            }
            
            fat12_write_fat_table();
            fat12_write_directory(current_dir_cluster, current_dir_entries);
            if (current_dir_cluster == 0) {
                fat12_reload_root_dir();
            }
            
            return 0;
        }
    }
    
    // File doesn't exist - create new
    // Calculate number of clusters needed
    uint32_t clusters_needed = (size + 511) / 512;
    if (clusters_needed == 0) clusters_needed = 1;
    
    // Allocate cluster chain
    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t free_cluster = fat12_find_free_cluster();
        if (free_cluster == 0) {
            klog_error("No free clusters available");
            // Free previously allocated clusters
            uint16_t cluster = first_cluster;
            while (cluster >= 2 && cluster < 0xFF8) {
                uint16_t next = fat12_get_next_cluster(cluster);
                fat12_set_cluster_value(cluster, 0x000);
                cluster = next;
            }
            return -1;
        }
        
        if (i == 0) {
            first_cluster = free_cluster;
        } else {
            fat12_set_cluster_value(prev_cluster, free_cluster);
        }
        
        prev_cluster = free_cluster;
    }
    
    // Mark last cluster as EOF
    fat12_set_cluster_value(prev_cluster, 0xFFF);
    
    // Find free entry in current directory
    int free_entry = -1;
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00 || (uint8_t)current_dir_entries[i].filename[0] == 0xE5) {
            free_entry = i;
            break;
        }
    }
    
    if (free_entry == -1) {
        klog_error("No free directory entries");
        return -1;
    }
    
    char formatted_name[11];
    fat12_parse_filename(name, formatted_name);
    
    for (int i = 0; i < 11; i++) {
        if (formatted_name[i] >= 'a' && formatted_name[i] <= 'z') {
            formatted_name[i] -= 32;
        }
    }
    
    memcpy(current_dir_entries[free_entry].filename, formatted_name, 8);
    memcpy(current_dir_entries[free_entry].extension, formatted_name + 8, 3);
    current_dir_entries[free_entry].attributes = FAT12_ATTR_ARCHIVE;
    current_dir_entries[free_entry].reserved = 0;
    current_dir_entries[free_entry].creation_time_tenths = 0;
    current_dir_entries[free_entry].creation_time = 0;
    current_dir_entries[free_entry].creation_date = 0;
    current_dir_entries[free_entry].last_access_date = 0;
    current_dir_entries[free_entry].first_cluster_high = 0;
    current_dir_entries[free_entry].last_modified_time = 0;
    current_dir_entries[free_entry].last_modified_date = 0;
    current_dir_entries[free_entry].first_cluster_low = first_cluster;
    current_dir_entries[free_entry].file_size = size;
    
    // Write data across cluster chain
    uint32_t data_start = bpb.reserved_sectors + 
                         (bpb.fat_count * bpb.sectors_per_fat) +
                         ((bpb.root_dir_entries * 32 + 511) / 512);
    
    uint16_t cluster = first_cluster;
    uint32_t bytes_written = 0;
    
    while (cluster >= 2 && cluster < 0xFF8 && bytes_written < size) {
        uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        uint8_t sector_data[512];
        memset(sector_data, 0, 512);
        
        uint32_t bytes_to_write = size - bytes_written;
        if (bytes_to_write > 512) bytes_to_write = 512;
        
        memcpy(sector_data, data + bytes_written, bytes_to_write);
        ata_write_sector(sector, sector_data);
        
        bytes_written += bytes_to_write;
        cluster = fat12_get_next_cluster(cluster);
    }
    
    fat12_write_fat_table();
    fat12_write_directory(current_dir_cluster, current_dir_entries);
    if (current_dir_cluster == 0) {
        fat12_reload_root_dir();
    }
    
    return 0;
}

int fat12_delete(const char* name) {
    if (!fat12_initialized) return -1;
    
    // Load current directory
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        if ((uint8_t)current_dir_entries[i].attributes == FAT12_ATTR_LFN) continue;
        
        if (fat12_entry_matches(current_dir_entries, i, name)) {
            uint16_t cluster = current_dir_entries[i].first_cluster_low;
            
            while (cluster >= 2 && cluster < 0xFF8) {
                uint16_t next_cluster = fat12_get_next_cluster(cluster);
                fat12_set_cluster_value(cluster, 0x000);
                cluster = next_cluster;
            }
            
            // Also mark LFN entries as deleted
            for (int k = i - 1; k >= 0; k--) {
                if ((uint8_t)current_dir_entries[k].attributes != FAT12_ATTR_LFN) break;
                current_dir_entries[k].filename[0] = 0xE5;
            }
            current_dir_entries[i].filename[0] = 0xE5;
            
            fat12_write_fat_table();
            fat12_write_directory(current_dir_cluster, current_dir_entries);
            if (current_dir_cluster == 0) {
                fat12_reload_root_dir();
            }
            
            return 0;
        }
    }
    
    klog_warn("File not found");
    return -1;
}

static fs_file_info_t fat12_file_list[224];

int fat12_list(void** entries, int* count) {
    if (!fat12_initialized) return -1;
    
    *count = 0;
    
    // Load current directory entries
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        if (current_dir_entries[i].attributes & FAT12_ATTR_VOLUME_ID) continue;
        // Skip LFN entries - they will be read via fat12_read_lfn
        if ((uint8_t)current_dir_entries[i].attributes == FAT12_ATTR_LFN) continue;
        // Skip hidden and system
        if (current_dir_entries[i].attributes & 0x02) continue;
        if (current_dir_entries[i].attributes & 0x04) continue;
        
        if (current_dir_entries[i].first_cluster_low == 0 && current_dir_entries[i].file_size > 0) continue;
        if (current_dir_entries[i].first_cluster_low == 0 && !(current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY)) {
            if (current_dir_entries[i].filename[0] == 0x00 || current_dir_entries[i].filename[0] == ' ') continue;
        }
        
        memset(fat12_file_list[*count].name, 0, FS_MAX_FILENAME);
        
        // Try to read LFN first
        char lfn_name[256];
        lfn_name[0] = '\0';
        fat12_read_lfn(current_dir_entries, i, lfn_name, 256);
        
        if (lfn_name[0] != '\0') {
            // Use long filename
            int k = 0;
            while (lfn_name[k] && k < FS_MAX_FILENAME - 1) {
                fat12_file_list[*count].name[k] = lfn_name[k];
                k++;
            }
            fat12_file_list[*count].name[k] = '\0';
        } else {
            // Fall back to 8.3 short name
            int name_pos = 0;
            for (int j = 0; j < 8 && current_dir_entries[i].filename[j] != ' '; j++) {
                fat12_file_list[*count].name[name_pos++] = current_dir_entries[i].filename[j];
            }
            if (current_dir_entries[i].extension[0] != ' ') {
                fat12_file_list[*count].name[name_pos++] = '.';
                for (int j = 0; j < 3 && current_dir_entries[i].extension[j] != ' '; j++) {
                    fat12_file_list[*count].name[name_pos++] = current_dir_entries[i].extension[j];
                }
            }
            fat12_file_list[*count].name[name_pos] = '\0';
        }
        
        fat12_file_list[*count].size = current_dir_entries[i].file_size;
        fat12_file_list[*count].used = 1;
        fat12_file_list[*count].is_directory = (current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY) ? 1 : 0;
        
        (*count)++;
    }
    
    *entries = (void*)fat12_file_list;
    return 0;
}

int fat12_mkdir(const char* name) {
    if (!fat12_initialized) return -1;
    
    // Validate name
    if (!name || name[0] == '\0') {
        klog_error("Invalid directory name");
        return -1;
    }
    
    // Prevent creating directories with reserved names
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "/") == 0) {
        klog_error("Cannot create directory with reserved name");
        return -1;
    }
    
    // Load current directory
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    // Check for duplicate names
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, current_dir_entries[i].filename, 8);
        memcpy(fat_filename + 8, current_dir_entries[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            klog_error("Directory or file already exists with this name");
            return -1;
        }
    }
    
    // Find free directory entry
    int free_entry = -1;
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00 || (uint8_t)current_dir_entries[i].filename[0] == 0xE5) {
            free_entry = i;
            break;
        }
    }
    
    if (free_entry == -1) {
        klog_error("No free directory entries");
        return -1;
    }
    
    // Find free cluster
    uint16_t free_cluster = fat12_find_free_cluster();
    if (free_cluster == 0) {
        klog_error("No free clusters available");
        return -1;
    }
    
    // Format directory name
    char formatted_name[11];
    fat12_parse_filename(name, formatted_name);
    for (int i = 0; i < 11; i++) {
        if (formatted_name[i] >= 'a' && formatted_name[i] <= 'z') {
            formatted_name[i] -= 32;
        }
    }
    
    // Create directory entry
    memcpy(current_dir_entries[free_entry].filename, formatted_name, 8);
    memcpy(current_dir_entries[free_entry].extension, formatted_name + 8, 3);
    current_dir_entries[free_entry].attributes = FAT12_ATTR_DIRECTORY;
    current_dir_entries[free_entry].reserved = 0;
    current_dir_entries[free_entry].creation_time_tenths = 0;
    current_dir_entries[free_entry].creation_time = 0;
    current_dir_entries[free_entry].creation_date = 0;
    current_dir_entries[free_entry].last_access_date = 0;
    current_dir_entries[free_entry].first_cluster_high = 0;
    current_dir_entries[free_entry].last_modified_time = 0;
    current_dir_entries[free_entry].last_modified_date = 0;
    current_dir_entries[free_entry].first_cluster_low = free_cluster;
    current_dir_entries[free_entry].file_size = 0;
    
    // Mark cluster as used
    fat12_set_cluster_value(free_cluster, 0xFFF);
    
    // Initialize the directory cluster with zeros
    uint32_t data_start = bpb.reserved_sectors + 
                         (bpb.fat_count * bpb.sectors_per_fat) +
                         ((bpb.root_dir_entries * 32 + 511) / 512);
    uint32_t sector = data_start + (free_cluster - 2) * bpb.sectors_per_cluster;
    
    uint8_t empty_sector[512];
    memset(empty_sector, 0, 512);
    ata_write_sector(sector, empty_sector);
    
    // Write FAT and directory
    fat12_write_fat_table();
    fat12_write_directory(current_dir_cluster, current_dir_entries);
    if (current_dir_cluster == 0) {
        fat12_reload_root_dir();
    }
    
    return 0;
}

int fat12_rmdir(const char* name) {
    if (!fat12_initialized) return -1;
    
    // Load current directory
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, current_dir_entries[i].filename, 8);
        memcpy(fat_filename + 8, current_dir_entries[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            if (!(current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY)) {
                klog_warn("Not a directory");
                return -1;
            }
            
            uint16_t cluster = current_dir_entries[i].first_cluster_low;
            
            // Free cluster chain
            while (cluster >= 2 && cluster < 0xFF8) {
                uint16_t next_cluster = fat12_get_next_cluster(cluster);
                fat12_set_cluster_value(cluster, 0x000);
                cluster = next_cluster;
            }
            
            // Mark directory entry as deleted
            current_dir_entries[i].filename[0] = 0xE5;
            
            fat12_write_fat_table();
            fat12_write_directory(current_dir_cluster, current_dir_entries);
            if (current_dir_cluster == 0) {
                fat12_reload_root_dir();
            }
            
            return 0;
        }
    }
    
    klog_warn("Directory not found");
    return -1;
}

int fat12_chdir(const char* name) {
    if (!fat12_initialized) return -1;
    
    // Handle special cases
    if (strcmp(name, "/") == 0) {
        strcpy(current_dir, "/");
        current_dir_cluster = 0;
        return 0;
    }
    
    if (strcmp(name, "..") == 0) {
        // Go to parent directory
        if (strcmp(current_dir, "/") == 0) {
            return 0; // Already at root
        }
        
        // Remove last directory from path
        char parent_path[256];
        strcpy(parent_path, current_dir);
        int len = strlen(parent_path);
        
        // Remove trailing slash if present
        if (len > 1 && parent_path[len - 1] == '/') {
            parent_path[len - 1] = '\0';
            len--;
        }
        
        // Find last slash
        int last_slash = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (parent_path[i] == '/') {
                last_slash = i;
                break;
            }
        }
        
        if (last_slash == 0) {
            // Parent is root
            strcpy(current_dir, "/");
            current_dir_cluster = 0;
            return 0;
        }
        
        // Truncate to parent path
        parent_path[last_slash + 1] = '\0';
        
        // Navigate from root to find parent cluster
        current_dir_cluster = 0;
        strcpy(current_dir, "/");
        
        // Parse parent path and navigate to it
        char path_copy[256];
        strcpy(path_copy, parent_path);
        
        char* token = path_copy + 1; // Skip leading /
        while (*token) {
            // Find next /
            char* next_slash = token;
            while (*next_slash && *next_slash != '/') next_slash++;
            
            if (token == next_slash) break; // Empty component
            
            // Extract directory name
            char dir_name[256];
            int name_len = next_slash - token;
            memcpy(dir_name, token, name_len);
            dir_name[name_len] = '\0';
            
            // Find this directory in current cluster
            fat12_load_directory(current_dir_cluster, current_dir_entries);
            int found = 0;
            
            for (int i = 0; i < 224; i++) {
                if (current_dir_entries[i].filename[0] == 0x00) break;
                if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
                
                char fat_filename[12];
                memcpy(fat_filename, current_dir_entries[i].filename, 8);
                memcpy(fat_filename + 8, current_dir_entries[i].extension, 3);
                fat_filename[11] = '\0';
                
                if (fat12_compare_filename(fat_filename, dir_name)) {
                    if (current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY) {
                        current_dir_cluster = current_dir_entries[i].first_cluster_low;
                        found = 1;
                        break;
                    }
                }
            }
            
            if (!found) {
                // Directory not found, go back to root
                strcpy(current_dir, "/");
                current_dir_cluster = 0;
                return -1;
            }
            
            // Move to next component
            if (*next_slash == '/') {
                token = next_slash + 1;
            } else {
                break;
            }
        }
        
        strcpy(current_dir, parent_path);
        return 0;
    }
    
    // Load current directory
    fat12_load_directory(current_dir_cluster, current_dir_entries);
    
    // Check if directory exists
    for (int i = 0; i < 224; i++) {
        if (current_dir_entries[i].filename[0] == 0x00) break;
        if ((uint8_t)current_dir_entries[i].filename[0] == 0xE5) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, current_dir_entries[i].filename, 8);
        memcpy(fat_filename + 8, current_dir_entries[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            if (!(current_dir_entries[i].attributes & FAT12_ATTR_DIRECTORY)) {
                klog_warn("Not a directory");
                return -1;
            }
            
            // Update current directory cluster and path
            current_dir_cluster = current_dir_entries[i].first_cluster_low;
            if (current_dir[strlen(current_dir) - 1] != '/') {
                strcat(current_dir, "/");
            }
            strcat(current_dir, name);
            return 0;
        }
    }
    
    klog_warn("Directory not found");
    return -1;
}

const char* fat12_getcwd(void) {
    return current_dir;
}
