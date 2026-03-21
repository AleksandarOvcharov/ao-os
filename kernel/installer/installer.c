#include "installer.h"
#include "ata.h"
#include "vga.h"
#include "string.h"
#include "klog.h"

static uint8_t sector_buffer[512];

int installer_detect_disk(void) {
    terminal_writestring("Detecting disk...\n");
    
    if (!ata_is_available()) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("ERROR: No ATA disk detected\n");
        uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(normal_color);
        return INSTALLER_NO_DISK;
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("Disk found\n");
    
    uint8_t info_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(info_color);
    terminal_writestring("Mode: ATA PIO\n");
    
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

int installer_prepare_disk(void) {
    terminal_writestring("Preparing disk...\n");
    terminal_writestring("Clearing first sectors...\n");
    
    memset(sector_buffer, 0, 512);
    
    for (int i = 0; i < 10; i++) {
        if (ata_write_sector(i, sector_buffer) != 0) {
            uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_setcolor(error_color);
            terminal_writestring("ERROR: Failed to clear sectors\n");
            uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_setcolor(normal_color);
            return INSTALLER_WRITE_ERROR;
        }
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("OK\n");
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

int installer_format_filesystem(void) {
    terminal_writestring("Formatting filesystem...\n");
    
    memset(sector_buffer, 0, 512);
    
    // Create FAT12 boot sector
    sector_buffer[0] = 0xEB;  // JMP
    sector_buffer[1] = 0x3C;
    sector_buffer[2] = 0x90;  // NOP
    
    // OEM name
    const char* oem = "AOOS1.0";
    for (int i = 0; i < 8; i++) {
        sector_buffer[3 + i] = oem[i];
    }
    
    // BPB (BIOS Parameter Block)
    *(uint16_t*)&sector_buffer[11] = 512;   // Bytes per sector
    sector_buffer[13] = 1;                   // Sectors per cluster
    *(uint16_t*)&sector_buffer[14] = 1;     // Reserved sectors
    sector_buffer[16] = 2;                   // Number of FATs
    *(uint16_t*)&sector_buffer[17] = 224;   // Root directory entries
    *(uint16_t*)&sector_buffer[19] = 2880;  // Total sectors (1.44MB)
    sector_buffer[21] = 0xF0;                // Media descriptor
    *(uint16_t*)&sector_buffer[22] = 9;     // Sectors per FAT
    *(uint16_t*)&sector_buffer[24] = 18;    // Sectors per track
    *(uint16_t*)&sector_buffer[26] = 2;     // Number of heads
    
    // Boot signature
    sector_buffer[510] = 0x55;
    sector_buffer[511] = 0xAA;
    
    // Write boot sector
    if (ata_write_sector(0, sector_buffer) != 0) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("ERROR: Failed to write boot sector\n");
        uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(normal_color);
        return INSTALLER_WRITE_ERROR;
    }
    
    // Create FAT tables
    memset(sector_buffer, 0, 512);
    sector_buffer[0] = 0xF0;  // Media descriptor
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF;
    
    // Write FAT1
    for (int i = 0; i < 9; i++) {
        if (ata_write_sector(1 + i, sector_buffer) != 0) {
            uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_setcolor(error_color);
            terminal_writestring("ERROR: Failed to write FAT1\n");
            uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_setcolor(normal_color);
            return INSTALLER_WRITE_ERROR;
        }
        memset(sector_buffer, 0, 512);
    }
    
    // Write FAT2
    sector_buffer[0] = 0xF0;
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF;
    for (int i = 0; i < 9; i++) {
        if (ata_write_sector(10 + i, sector_buffer) != 0) {
            uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_setcolor(error_color);
            terminal_writestring("ERROR: Failed to write FAT2\n");
            uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_setcolor(normal_color);
            return INSTALLER_WRITE_ERROR;
        }
        memset(sector_buffer, 0, 512);
    }
    
    // Create root directory
    memset(sector_buffer, 0, 512);
    for (int i = 0; i < 14; i++) {
        if (ata_write_sector(19 + i, sector_buffer) != 0) {
            uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_setcolor(error_color);
            terminal_writestring("ERROR: Failed to write root directory\n");
            uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_setcolor(normal_color);
            return INSTALLER_WRITE_ERROR;
        }
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("OK\n");
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

static int installer_create_welcome_files(void) {
    terminal_writestring("Creating welcome files...\n");
    
    uint8_t root_sector[512];
    uint32_t root_dir_start = 19;
    
    // Read root directory
    if (ata_read_sector(root_dir_start, root_sector) != 0) {
        return INSTALLER_READ_ERROR;
    }
    
    // Create WELCOME.TXT entry
    memset(&root_sector[0], ' ', 11);
    memcpy(&root_sector[0], "WELCOME ", 8);
    memcpy(&root_sector[8], "TXT", 3);
    root_sector[11] = 0x20;  // Archive attribute
    *(uint16_t*)&root_sector[26] = 2;  // First cluster
    
    const char* welcome_msg = "Welcome to AO OS!\r\n\r\nYou have successfully installed AO OS to disk.\r\n\r\nTry these commands:\r\n  ls - List files\r\n  cat WELCOME.TXT - Read this file\r\n  edit test.txt - Create a file\r\n  help - Show all commands\r\n";
    uint32_t welcome_len = 0;
    while (welcome_msg[welcome_len]) welcome_len++;
    *(uint32_t*)&root_sector[28] = welcome_len;
    
    // Create README.TXT entry
    memset(&root_sector[32], ' ', 11);
    memcpy(&root_sector[32], "README  ", 8);
    memcpy(&root_sector[40], "TXT", 3);
    root_sector[43] = 0x20;  // Archive attribute
    *(uint16_t*)&root_sector[58] = 3;  // First cluster
    
    const char* readme_msg = "AO OS - A simple operating system\r\n\r\nVersion: 0.7.0\r\nCodename: Nebula\r\n\r\nFeatures:\r\n- FAT12 filesystem\r\n- Text editor\r\n- Shell commands\r\n- Disk installer\r\n";
    uint32_t readme_len = 0;
    while (readme_msg[readme_len]) readme_len++;
    *(uint32_t*)&root_sector[60] = readme_len;
    
    // Write root directory
    if (ata_write_sector(root_dir_start, root_sector) != 0) {
        return INSTALLER_WRITE_ERROR;
    }
    
    // Write WELCOME.TXT content to cluster 2 (sector 33)
    memset(sector_buffer, 0, 512);
    memcpy(sector_buffer, welcome_msg, welcome_len);
    if (ata_write_sector(33, sector_buffer) != 0) {
        return INSTALLER_WRITE_ERROR;
    }
    
    // Write README.TXT content to cluster 3 (sector 34)
    memset(sector_buffer, 0, 512);
    memcpy(sector_buffer, readme_msg, readme_len);
    if (ata_write_sector(34, sector_buffer) != 0) {
        return INSTALLER_WRITE_ERROR;
    }
    
    // Update FAT to mark clusters as used
    uint8_t fat_sector[512];
    if (ata_read_sector(1, fat_sector) != 0) {
        return INSTALLER_READ_ERROR;
    }
    
    // Mark cluster 2 as EOF
    fat_sector[3] = 0xFF;
    fat_sector[4] = 0x0F;
    
    // Mark cluster 3 as EOF
    fat_sector[4] |= 0xF0;
    fat_sector[5] = 0xFF;
    
    // Write FAT1
    if (ata_write_sector(1, fat_sector) != 0) {
        return INSTALLER_WRITE_ERROR;
    }
    
    // Write FAT2
    if (ata_write_sector(10, fat_sector) != 0) {
        return INSTALLER_WRITE_ERROR;
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("OK\n");
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

int installer_install_kernel(void) {
    terminal_writestring("Installing kernel...\n");
    terminal_writestring("Writing kernel to disk...\n");
    
    // For now, just write a placeholder
    // In a real implementation, you would read the kernel from memory
    memset(sector_buffer, 0, 512);
    const char* kernel_marker = "AO-KERNEL";
    for (int i = 0; kernel_marker[i]; i++) {
        sector_buffer[i] = kernel_marker[i];
    }
    
    // Write kernel starting at sector 35 (after welcome files)
    if (ata_write_sector(35, sector_buffer) != 0) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("ERROR: Failed to write kernel\n");
        uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(normal_color);
        return INSTALLER_WRITE_ERROR;
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("OK\n");
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

int installer_install_bootloader(void) {
    terminal_writestring("Installing bootloader...\n");
    
    // Read current boot sector
    if (ata_read_sector(0, sector_buffer) != 0) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("ERROR: Failed to read boot sector\n");
        uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(normal_color);
        return INSTALLER_READ_ERROR;
    }
    
    // Add simple bootloader code (just a placeholder for now)
    // In a real implementation, this would be actual bootloader code
    const char* boot_msg = "AO OS Boot";
    for (int i = 0; boot_msg[i] && i < 50; i++) {
        sector_buffer[100 + i] = boot_msg[i];
    }
    
    // Write back boot sector
    if (ata_write_sector(0, sector_buffer) != 0) {
        uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_setcolor(error_color);
        terminal_writestring("ERROR: Failed to write bootloader\n");
        uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_setcolor(normal_color);
        return INSTALLER_WRITE_ERROR;
    }
    
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("OK\n");
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    return INSTALLER_OK;
}

int installer_run(void) {
    uint8_t title_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_setcolor(title_color);
    terminal_writestring("\n");
    terminal_writestring("================================================================================\n");
    terminal_writestring("                            AO OS Installer v1.0                               \n");
    terminal_writestring("================================================================================\n");
    terminal_writestring("\n");
    
    uint8_t normal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_setcolor(normal_color);
    
    // Step 1: Detect disk
    if (installer_detect_disk() != INSTALLER_OK) {
        return INSTALLER_NO_DISK;
    }
    terminal_writestring("\n");
    
    // Step 2: Prepare disk
    if (installer_prepare_disk() != INSTALLER_OK) {
        return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("\n");
    
    // Step 3: Format filesystem
    if (installer_format_filesystem() != INSTALLER_OK) {
        return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("\n");
    
    // Step 4: Create welcome files
    if (installer_create_welcome_files() != INSTALLER_OK) {
        return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("\n");
    
    // Step 5: Install kernel
    if (installer_install_kernel() != INSTALLER_OK) {
        return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("\n");
    
    // Step 6: Install bootloader
    if (installer_install_bootloader() != INSTALLER_OK) {
        return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("\n");
    
    // Success message
    terminal_setcolor(title_color);
    terminal_writestring("================================================================================\n");
    uint8_t success_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_setcolor(success_color);
    terminal_writestring("                       Installation complete!                                  \n");
    terminal_setcolor(title_color);
    terminal_writestring("================================================================================\n");
    
    terminal_setcolor(normal_color);
    terminal_writestring("\n");
    terminal_writestring("Rebooting in 3 seconds...\n");
    terminal_writestring("\n");
    
    klog_info("Installation completed successfully");
    
    // Wait 3 seconds before reboot
    extern void timer_wait(uint32_t ticks);
    timer_wait(300);  // 3 seconds at 100Hz
    
    // Reboot
    extern void system_reboot(void);
    system_reboot();
    
    return INSTALLER_OK;
}
