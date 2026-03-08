#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_START 0x00100000
#define HEAP_SIZE  0x00100000
#define BLOCK_SIZE 16

typedef struct memory_block {
    size_t size;
    int is_free;
    struct memory_block* next;
} memory_block_t;

void memory_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t memory_get_free(void);
size_t memory_get_used(void);

#endif
