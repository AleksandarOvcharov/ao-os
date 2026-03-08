#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>

#define FAT12_SECTOR_SIZE 512
#define FAT12_MAX_FILENAME 11
#define FAT12_ATTR_READ_ONLY 0x01
#define FAT12_ATTR_HIDDEN 0x02
#define FAT12_ATTR_SYSTEM 0x04
#define FAT12_ATTR_VOLUME_ID 0x08
#define FAT12_ATTR_DIRECTORY 0x10
#define FAT12_ATTR_ARCHIVE 0x20

typedef struct {
    uint8_t jump[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t large_sector_count;
} __attribute__((packed)) fat12_bpb_t;

typedef struct {
    char filename[8];
    char extension[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_modified_time;
    uint16_t last_modified_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat12_dir_entry_t;

void fat12_init(void);
int fat12_available(void);
int fat12_create(const char* name, const char* data, uint32_t size);
int fat12_read(const char* name, char* buffer, uint32_t* size);
int fat12_delete(const char* name);
int fat12_list(void** entries, int* count);

#endif
