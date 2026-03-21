#include "aob.h"
#include "fs.h"
#include "klog.h"
#include "string.h"
#include "vga.h"
#include "process.h"

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
    memcpy(ctx->name, header->name, 32);

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

    /* Copy code to USER_CODE_BASE (0x200000) */
    uint8_t* exec_addr = (uint8_t*)USER_CODE_BASE;
    memcpy(exec_addr, ctx->code_ptr, ctx->code_size);

    /* Write kernel API pointers at USER_API_BASE (0x90000).
     * Note: in ring 3, user programs cannot call these directly -
     * they must use int 0x80 syscalls instead. Kept for info only. */
    typedef void (*putchar_fn)(char);
    typedef void (*writestring_fn)(const char*);
    typedef void (*setcolor_fn)(uint8_t);
    typedef void (*clear_fn)(void);
    volatile uintptr_t* api = (volatile uintptr_t*)USER_API_BASE;
    api[0] = AO_API_MAGIC;
    api[1] = (uintptr_t)(putchar_fn)terminal_putchar;
    api[2] = (uintptr_t)(writestring_fn)terminal_writestring;
    api[3] = (uintptr_t)(setcolor_fn)terminal_setcolor;
    api[4] = (uintptr_t)(clear_fn)terminal_clear;

    /* Create user process in ring 3 */
    uint64_t entry = (uint64_t)(uintptr_t)(exec_addr + ctx->entry_point);
    const char* pname = ctx->name[0] ? ctx->name : "aob";
    process_t* proc = process_create(pname, entry, 1);
    if (!proc) {
        klog_error("Failed to create process for AOB");
        return -1;
    }

    /* Wait for process to complete */
    process_wait(proc->pid);

    return 0;
}

void aob_unload(aob_context_t* ctx) {
    if (ctx) {
        ctx->code_ptr = 0;
        ctx->code_size = 0;
        ctx->entry_point = 0;
    }
}
