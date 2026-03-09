#include "aob.h"
#include "fs.h"
#include "klog.h"
#include "string.h"
#include "vga.h"

// Kernel API magic - must match ao.h
#define AO_API_MAGIC 0x414F4150

static char aob_buffer[16384];

int aob_load(const char* filename, aob_context_t* ctx) {
    if (!filename || !ctx) {
        klog_error("Invalid parameters for aob_load");
        return -1;
    }
    
    uint32_t file_size;
    if (fs_read(filename, aob_buffer, &file_size) != 0) {
        klog_error("Failed to read AOB file");
        return -1;
    }
    
    if (file_size < sizeof(aob_header_t)) {
        klog_error("File too small to be valid AOB");
        return -1;
    }
    
    aob_header_t* header = (aob_header_t*)aob_buffer;
    
    if (header->magic != AOB_MAGIC) {
        klog_error("Invalid AOB magic number");
        return -1;
    }
    
    if (header->version != AOB_VERSION) {
        klog_error("Unsupported AOB version");
        return -1;
    }
    
    if (header->code_size == 0) {
        klog_error("AOB has no code");
        return -1;
    }
    
    if (file_size < sizeof(aob_header_t) + header->code_size) {
        klog_error("File size mismatch");
        return -1;
    }
    
    ctx->code_ptr = (void*)(aob_buffer + sizeof(aob_header_t));
    ctx->code_size = header->code_size;
    ctx->entry_point = header->entry_point;
    
    return 0;
}

int aob_execute(aob_context_t* ctx) {
    if (!ctx || !ctx->code_ptr) {
        klog_error("Invalid AOB context");
        return -1;
    }
    
    if (ctx->entry_point >= ctx->code_size) {
        klog_error("Invalid entry point");
        return -1;
    }
    
    // Copy code to fixed address 0x200000 so string literals resolve correctly
    // (compiled with -Ttext=0x200000)
    uint8_t* exec_addr = (uint8_t*)0x00200000;
    memcpy(exec_addr, ctx->code_ptr, ctx->code_size);
    
    // Write kernel API pointers to fixed address 0x00090000
    typedef void (*putchar_fn)(char);
    typedef void (*writestring_fn)(const char*);
    typedef void (*setcolor_fn)(uint8_t);
    typedef void (*clear_fn)(void);
    volatile unsigned int* api = (volatile unsigned int*)0x00090000;
    api[0] = AO_API_MAGIC;
    api[1] = (unsigned int)(putchar_fn)terminal_putchar;
    api[2] = (unsigned int)(writestring_fn)terminal_writestring;
    api[3] = (unsigned int)(setcolor_fn)terminal_setcolor;
    api[4] = (unsigned int)(clear_fn)terminal_clear;
    
    void (*entry_func)(void) = (void (*)(void))(exec_addr + ctx->entry_point);
    
    entry_func();
    
    return 0;
}

void aob_unload(aob_context_t* ctx) {
    if (ctx) {
        ctx->code_ptr = 0;
        ctx->code_size = 0;
        ctx->entry_point = 0;
    }
}
