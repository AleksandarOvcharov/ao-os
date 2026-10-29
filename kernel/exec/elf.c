#include "elf.h"
#include "fs.h"
#include "klog.h"
#include "process.h"
#include "string.h"
#include "vga.h"
#include "vmm.h"

#define EI_CLASS      4
#define EI_DATA       5
#define ELFCLASS64    2
#define ELFDATA2LSB   1
#define ET_EXEC       2
#define EM_X86_64     62
#define EV_CURRENT    1
#define PT_LOAD       1
#define PF_W          2

#define ELF_LOAD_BUFFER_SIZE 131072

static uint8_t elf_buffer[ELF_LOAD_BUFFER_SIZE];

static uint64_t page_down(uint64_t v) {
    return v & ~(uint64_t)(4096 - 1);
}

static uint64_t page_up(uint64_t v) {
    return (v + 4095) & ~(uint64_t)(4096 - 1);
}

static int map_range(process_t* proc, uint64_t start, uint64_t end, uint32_t ph_flags) {
    uint64_t flags = PTE_USER;
    if (ph_flags & PF_W) flags |= PTE_WRITABLE;

    for (uint64_t addr = page_down(start); addr < page_up(end); addr += 4096) {
        if (process_map_user_page(proc, addr, flags) != 0) return -1;
    }
    return 0;
}

static int valid_user_range(uint64_t vaddr, uint64_t size) {
    if (size == 0) return 1;
    if (vaddr < 0x10000) return 0;
    if (vaddr >= USER_STACK_BASE) return 0;
    if (vaddr + size < vaddr) return 0;
    if (vaddr + size > USER_STACK_BASE) return 0;
    if (vaddr <= USER_API_BASE + 4096 && vaddr + size > USER_API_BASE) return 0;
    if (vaddr <= USER_ARGV_BASE + 4096 && vaddr + size > USER_ARGV_BASE) return 0;
    return 1;
}

int elf_spawn(const char* filename, const char* cmdline, int background) {
    if (!filename) return -1;

    uint32_t file_size = 0;
    if (fs_read(filename, (char*)elf_buffer, &file_size, sizeof(elf_buffer)) != 0) {
        klog_error("Failed to read ELF file");
        return -1;
    }
    if (file_size < sizeof(elf64_header_t) || file_size > ELF_LOAD_BUFFER_SIZE) {
        klog_error("Invalid ELF file size");
        return -1;
    }

    elf64_header_t* eh = (elf64_header_t*)elf_buffer;
    if (*(uint32_t*)eh->ident != ELF_MAGIC ||
        eh->ident[EI_CLASS] != ELFCLASS64 ||
        eh->ident[EI_DATA] != ELFDATA2LSB ||
        eh->type != ET_EXEC ||
        eh->machine != EM_X86_64 ||
        eh->version != EV_CURRENT) {
        klog_error("Unsupported ELF executable");
        return -1;
    }
    if (eh->phentsize != sizeof(elf64_phdr_t) || eh->phnum == 0) {
        klog_error("ELF has invalid program headers");
        return -1;
    }
    if (eh->phoff + (uint64_t)eh->phnum * eh->phentsize > file_size) {
        klog_error("ELF program headers out of range");
        return -1;
    }
    if (!valid_user_range(eh->entry, 1)) {
        klog_error("ELF entry point outside user range");
        return -1;
    }

    char pname[32];
    strncpy(pname, filename, sizeof(pname) - 1);
    pname[sizeof(pname) - 1] = '\0';

    process_t* proc = process_create(pname, eh->entry, 1);
    if (!proc) return -1;

    elf64_phdr_t* ph = (elf64_phdr_t*)(elf_buffer + eh->phoff);
    for (uint16_t i = 0; i < eh->phnum; i++) {
        if (ph[i].type != PT_LOAD) continue;
        if (ph[i].memsz < ph[i].filesz ||
            ph[i].offset + ph[i].filesz > file_size ||
            !valid_user_range(ph[i].vaddr, ph[i].memsz)) {
            process_terminate(proc->pid);
            process_wait(proc->pid);
            klog_error("Invalid ELF load segment");
            return -1;
        }

        if (map_range(proc, ph[i].vaddr, ph[i].vaddr + ph[i].memsz, ph[i].flags) != 0 ||
            process_write_user(proc, ph[i].vaddr, elf_buffer + ph[i].offset, ph[i].filesz) != 0 ||
            process_zero_user(proc, ph[i].vaddr + ph[i].filesz, ph[i].memsz - ph[i].filesz) != 0) {
            process_terminate(proc->pid);
            process_wait(proc->pid);
            klog_error("Failed to map ELF segment");
            return -1;
        }
    }

    uintptr_t api[8];
    memset(api, 0, sizeof(api));
    api[0] = 0x414F4150;
    process_write_user(proc, USER_API_BASE, api, sizeof(api));
    process_setup_user_args(proc, cmdline ? cmdline : filename);

    if (background) {
        process_detach(proc->pid);
        return (int)proc->pid;
    }

    process_wait(proc->pid);
    return 0;
}

int elf_execute(const char* filename, const char* cmdline) {
    return elf_spawn(filename, cmdline, 0);
}
