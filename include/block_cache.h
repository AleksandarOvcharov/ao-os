#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include <stdint.h>

void block_cache_init(void);
void block_cache_invalidate(void);
int block_cache_read_sector(uint32_t lba, uint8_t* buffer);
int block_cache_write_sector(uint32_t lba, const uint8_t* buffer);
void block_cache_flush(void);

#endif
