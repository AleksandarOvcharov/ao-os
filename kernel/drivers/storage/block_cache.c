#include "block_cache.h"
#include "ata.h"
#include "string.h"

#define BLOCK_CACHE_SIZE 32
#define SECTOR_SIZE 512

typedef struct {
    uint8_t valid;
    uint32_t lba;
    uint32_t age;
    uint8_t data[SECTOR_SIZE];
} cache_entry_t;

static cache_entry_t cache[BLOCK_CACHE_SIZE];
static uint32_t clock_tick;

void block_cache_init(void) {
    block_cache_invalidate();
}

void block_cache_invalidate(void) {
    memset(cache, 0, sizeof(cache));
    clock_tick = 1;
}

static int find_entry(uint32_t lba) {
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].lba == lba) return i;
    }
    return -1;
}

static int choose_victim(void) {
    int victim = 0;
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (!cache[i].valid) return i;
        if (cache[i].age < cache[victim].age) victim = i;
    }
    return victim;
}

int block_cache_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!buffer) return -1;
    int idx = find_entry(lba);
    if (idx >= 0) {
        cache[idx].age = clock_tick++;
        memcpy(buffer, cache[idx].data, SECTOR_SIZE);
        return 0;
    }

    idx = choose_victim();
    if (ata_read_sector(lba, cache[idx].data) != 0) return -1;
    cache[idx].valid = 1;
    cache[idx].lba = lba;
    cache[idx].age = clock_tick++;
    memcpy(buffer, cache[idx].data, SECTOR_SIZE);
    return 0;
}

int block_cache_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!buffer) return -1;
    if (ata_write_sector(lba, (uint8_t*)buffer) != 0) return -1;

    int idx = find_entry(lba);
    if (idx < 0) idx = choose_victim();
    cache[idx].valid = 1;
    cache[idx].lba = lba;
    cache[idx].age = clock_tick++;
    memcpy(cache[idx].data, buffer, SECTOR_SIZE);
    return 0;
}

void block_cache_flush(void) {
    /* Write-through cache: writes are committed immediately. */
}
