#include "memory.h"
#include "panic.h"
#include "string.h"
#include "pmm.h"

/* Linker-provided symbol: end of kernel image (page-aligned) */
extern char _kernel_end[];

static memory_block_t* heap_start = NULL;
static uint64_t heap_base = 0;
static size_t total_memory = 0;
static size_t used_memory = 0;

void memory_init(void) {
    /* Place heap right after the kernel BSS, page-aligned */
    heap_base = ((uint64_t)(uintptr_t)_kernel_end + 0xFFF) & ~0xFFFULL;
    heap_start = (memory_block_t*)(uintptr_t)heap_base;
    heap_start->size = HEAP_SIZE - sizeof(memory_block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;

    total_memory = HEAP_SIZE;
    used_memory = 0;

    /* Reserve heap pages in PMM so they're not given to anyone else */
    pmm_mark_region_used(heap_base, HEAP_SIZE);
}

static memory_block_t* find_free_block(size_t size) {
    memory_block_t* current = heap_start;

    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static void split_block(memory_block_t* block, size_t size) {
    if (block->size >= size + sizeof(memory_block_t) + BLOCK_SIZE) {
        memory_block_t* new_block = (memory_block_t*)((uint8_t*)block + sizeof(memory_block_t) + size);
        new_block->size = block->size - size - sizeof(memory_block_t);
        new_block->is_free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

static void merge_free_blocks(void) {
    memory_block_t* current = heap_start;

    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(memory_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t aligned_size = (size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);

    memory_block_t* block = find_free_block(aligned_size);

    if (block == NULL) {
        return NULL;
    }

    split_block(block, aligned_size);

    block->is_free = 0;
    used_memory += block->size + sizeof(memory_block_t);

    return (void*)((uint8_t*)block + sizeof(memory_block_t));
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    memory_block_t* block = (memory_block_t*)((uint8_t*)ptr - sizeof(memory_block_t));

    if ((uint8_t*)block < (uint8_t*)(uintptr_t)heap_base ||
        (uint8_t*)block >= (uint8_t*)(uintptr_t)(heap_base + HEAP_SIZE)) {
        panic("Invalid pointer passed to kfree");
        return;
    }

    if (block->is_free) {
        panic("Double free detected");
        return;
    }

    block->is_free = 1;
    used_memory -= block->size + sizeof(memory_block_t);

    merge_free_blocks();
}

size_t memory_get_free(void) {
    return total_memory - used_memory;
}

size_t memory_get_used(void) {
    return used_memory;
}

uint64_t memory_get_heap_start(void) {
    return heap_base;
}
