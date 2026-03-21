#ifndef AOB_H
#define AOB_H

#include <stdint.h>

// AOB Magic Number: "AOB\0"
#define AOB_MAGIC 0x00424F41

// AOB File Format Version
#define AOB_VERSION 1

// AOB Header Structure
typedef struct {
    uint32_t magic;           // Magic number: 0x00424F41 ("AOB\0")
    uint16_t version;         // Format version
    uint16_t flags;           // Flags (reserved for future use)
    uint32_t entry_point;     // Entry point offset from code start
    uint32_t code_size;       // Size of code section in bytes
    uint32_t data_size;       // Size of data section in bytes (reserved)
    uint32_t bss_size;        // Size of BSS section in bytes (reserved)
    char name[32];            // Program name
    uint32_t reserved[4];     // Reserved for future use
} __attribute__((packed)) aob_header_t;

// AOB execution context
typedef struct {
    void* code_ptr;           // Pointer to loaded code
    uint32_t code_size;       // Size of code
    uint32_t entry_point;     // Entry point offset
    char name[32];            // Program name from header
} aob_context_t;

// AOB loader functions
int aob_load(const char* filename, aob_context_t* ctx);
int aob_execute(aob_context_t* ctx);
void aob_unload(aob_context_t* ctx);

#endif
