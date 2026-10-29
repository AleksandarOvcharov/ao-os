#include "aob.h"
#include "fs.h"
#include "klog.h"
#include "string.h"
#include "vga.h"
#include "process.h"
#include "vmm.h"

// Kernel API magic - must match ao.h
#define AO_API_MAGIC 0x414F4150

static char aob_buffer[16384];

int aob_load(const char* filename, aob_context_t* ctx) {
    if (!filename || !ctx) {
        klog_error("Invalid parameters for aob_load");
        return -1;
    }
    
    uint32_t file_size;
    if (fs_read(filename, aob_buffer, &file_size, sizeof(aob_buffer)) != 0) {
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

int aob_spawn(aob_context_t* ctx, const char* cmdline, int background) {
    if (!ctx || !ctx->code_ptr) {
        klog_error("Invalid AOB context");
        return -1;
    }

    if (ctx->entry_point >= ctx->code_size) {
        klog_error("Invalid entry point");
        return -1;
    }

    /* Create user process in ring 3 */
    uint64_t entry = USER_CODE_BASE + ctx->entry_point;
    const char* pname = ctx->name[0] ? ctx->name : "aob";
    process_t* proc = process_create(pname, entry, 1);
    if (!proc) {
        klog_error("Failed to create process for AOB");
        return -1;
    }

    uint32_t pages = (ctx->code_size + 4095) / 4096;
    if (pages == 0) pages = 1;
    for (uint32_t i = 0; i < pages; i++) {
        if (process_map_user_page(proc, USER_CODE_BASE + (uint64_t)i * 4096, PTE_WRITABLE) != 0) {
            process_terminate(proc->pid);
            return -1;
        }
    }

    if (process_write_user(proc, USER_CODE_BASE, ctx->code_ptr, ctx->code_size) != 0) {
        process_terminate(proc->pid);
        return -1;
    }

    /* Write kernel API pointers at USER_API_BASE (0x90000).
     * In ring 3, programs should use int 0x80 syscalls; this page is retained
     * as a discovery/info page for existing AOB examples. */
    uintptr_t api[8];
    memset(api, 0, sizeof(api));
    api[0] = AO_API_MAGIC;
    api[1] = (uintptr_t)terminal_putchar;
    api[2] = (uintptr_t)terminal_writestring;
    api[3] = (uintptr_t)terminal_setcolor;
    api[4] = (uintptr_t)terminal_clear;
    process_write_user(proc, USER_API_BASE, api, sizeof(api));
    process_setup_user_args(proc, cmdline ? cmdline : pname);

    if (background) {
        process_detach(proc->pid);
        return (int)proc->pid;
    }

    process_wait(proc->pid);

    return 0;
}

int aob_execute(aob_context_t* ctx, const char* cmdline) {
    return aob_spawn(ctx, cmdline, 0);
}

void aob_unload(aob_context_t* ctx) {
    if (ctx) {
        ctx->code_ptr = 0;
        ctx->code_size = 0;
        ctx->entry_point = 0;
    }
}
