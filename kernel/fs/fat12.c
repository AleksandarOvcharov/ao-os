#include "fat12.h"
#include "fs.h"
#include "ata.h"
#include "string.h"
#include "klog.h"
#include "vga.h"

static fat12_bpb_t bpb;
static uint8_t fat_table[FAT12_SECTOR_SIZE * 9];
static fat12_dir_entry_t root_dir[224];
static int fat12_initialized = 0;

static uint16_t fat12_get_next_cluster(uint16_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2);
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

static int fat12_find_free_dir_entry(void) {
    for (int i = 0; i < bpb.root_dir_entries; i++) {
        if (root_dir[i].filename[0] == 0x00 || (uint8_t)root_dir[i].filename[0] == 0xE5) {
            return i;
        }
    }
    return -1;
}

static void fat12_write_fat_table(void) {
    uint32_t fat_start = bpb.reserved_sectors;
    uint32_t fat_size = bpb.sectors_per_fat;
    
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_write_sector(fat_start + i, &fat_table[i * 512]);
    }
    
    for (uint32_t i = 0; i < fat_size && i < 9; i++) {
        ata_write_sector(fat_start + fat_size + i, &fat_table[i * 512]);
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
        klog_warn("Non-standard sector size, using ramfs");
        fat12_initialized = 0;
        return;
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
    
    for (int i = 0; i < bpb.root_dir_entries; i++) {
        if (root_dir[i].filename[0] == 0x00) break;
        if ((uint8_t)root_dir[i].filename[0] == 0xE5) continue;
        if (root_dir[i].attributes & FAT12_ATTR_DIRECTORY) continue;
        if (root_dir[i].attributes & FAT12_ATTR_VOLUME_ID) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, root_dir[i].filename, 8);
        memcpy(fat_filename + 8, root_dir[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            uint32_t file_size = root_dir[i].file_size;
            if (file_size > 512) file_size = 512;
            
            uint16_t cluster = root_dir[i].first_cluster_low;
            uint32_t data_start = bpb.reserved_sectors + 
                                 (bpb.fat_count * bpb.sectors_per_fat) +
                                 ((bpb.root_dir_entries * 32 + 511) / 512);
            
            if (cluster >= 2) {
                uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
                uint8_t sector_data[512];
                ata_read_sector(sector, sector_data);
                
                memcpy(buffer, sector_data, file_size);
                *size = file_size;
                return 0;
            }
        }
    }
    
    return -1;
}

int fat12_create(const char* name, const char* data, uint32_t size) {
    if (!fat12_initialized) return -1;
    
    if (size > 512) {
        klog_warn("File too large for single cluster");
        size = 512;
    }
    
    for (int i = 0; i < bpb.root_dir_entries; i++) {
        if (root_dir[i].filename[0] == 0x00) break;
        if ((uint8_t)root_dir[i].filename[0] == 0xE5) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, root_dir[i].filename, 8);
        memcpy(fat_filename + 8, root_dir[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            uint16_t cluster = root_dir[i].first_cluster_low;
            
            if (cluster >= 2) {
                uint32_t data_start = bpb.reserved_sectors + 
                                     (bpb.fat_count * bpb.sectors_per_fat) +
                                     ((bpb.root_dir_entries * 32 + 511) / 512);
                uint32_t sector = data_start + (cluster - 2) * bpb.sectors_per_cluster;
                
                uint8_t sector_data[512];
                memset(sector_data, 0, 512);
                memcpy(sector_data, data, size);
                ata_write_sector(sector, sector_data);
                
                root_dir[i].file_size = size;
                fat12_write_root_dir();
                
                return 0;
            }
        }
    }
    
    uint16_t free_cluster = fat12_find_free_cluster();
    if (free_cluster == 0) {
        klog_error("No free clusters available");
        return -1;
    }
    
    int free_entry = fat12_find_free_dir_entry();
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
    
    memcpy(root_dir[free_entry].filename, formatted_name, 8);
    memcpy(root_dir[free_entry].extension, formatted_name + 8, 3);
    root_dir[free_entry].attributes = FAT12_ATTR_ARCHIVE;
    root_dir[free_entry].reserved = 0;
    root_dir[free_entry].creation_time_tenths = 0;
    root_dir[free_entry].creation_time = 0;
    root_dir[free_entry].creation_date = 0;
    root_dir[free_entry].last_access_date = 0;
    root_dir[free_entry].first_cluster_high = 0;
    root_dir[free_entry].last_modified_time = 0;
    root_dir[free_entry].last_modified_date = 0;
    root_dir[free_entry].first_cluster_low = free_cluster;
    root_dir[free_entry].file_size = size;
    
    fat12_set_cluster_value(free_cluster, 0xFFF);
    
    uint32_t data_start = bpb.reserved_sectors + 
                         (bpb.fat_count * bpb.sectors_per_fat) +
                         ((bpb.root_dir_entries * 32 + 511) / 512);
    uint32_t sector = data_start + (free_cluster - 2) * bpb.sectors_per_cluster;
    
    uint8_t sector_data[512];
    memset(sector_data, 0, 512);
    memcpy(sector_data, data, size);
    ata_write_sector(sector, sector_data);
    
    fat12_write_fat_table();
    fat12_write_root_dir();
    
    klog_info("File created successfully");
    return 0;
}

int fat12_delete(const char* name) {
    if (!fat12_initialized) return -1;
    
    for (int i = 0; i < bpb.root_dir_entries; i++) {
        if (root_dir[i].filename[0] == 0x00) break;
        if ((uint8_t)root_dir[i].filename[0] == 0xE5) continue;
        
        char fat_filename[12];
        memcpy(fat_filename, root_dir[i].filename, 8);
        memcpy(fat_filename + 8, root_dir[i].extension, 3);
        fat_filename[11] = '\0';
        
        if (fat12_compare_filename(fat_filename, name)) {
            uint16_t cluster = root_dir[i].first_cluster_low;
            
            while (cluster >= 2 && cluster < 0xFF8) {
                uint16_t next_cluster = fat12_get_next_cluster(cluster);
                fat12_set_cluster_value(cluster, 0x000);
                cluster = next_cluster;
            }
            
            root_dir[i].filename[0] = 0xE5;
            
            fat12_write_fat_table();
            fat12_write_root_dir();
            
            klog_info("File deleted successfully");
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
    
    for (int i = 0; i < bpb.root_dir_entries; i++) {
        if (root_dir[i].filename[0] == 0x00) break;
        if ((uint8_t)root_dir[i].filename[0] == 0xE5) continue;
        if (root_dir[i].attributes & FAT12_ATTR_DIRECTORY) continue;
        if (root_dir[i].attributes & FAT12_ATTR_VOLUME_ID) continue;
        
        memset(fat12_file_list[*count].name, 0, FS_MAX_FILENAME);
        
        int name_pos = 0;
        for (int j = 0; j < 8 && root_dir[i].filename[j] != ' '; j++) {
            fat12_file_list[*count].name[name_pos++] = root_dir[i].filename[j];
        }
        
        if (root_dir[i].extension[0] != ' ') {
            fat12_file_list[*count].name[name_pos++] = '.';
            for (int j = 0; j < 3 && root_dir[i].extension[j] != ' '; j++) {
                fat12_file_list[*count].name[name_pos++] = root_dir[i].extension[j];
            }
        }
        fat12_file_list[*count].name[name_pos] = '\0';
        
        fat12_file_list[*count].size = root_dir[i].file_size;
        fat12_file_list[*count].used = 1;
        
        (*count)++;
    }
    
    *entries = (void*)fat12_file_list;
    return 0;
}
