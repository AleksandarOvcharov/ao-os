#include "installer.h"
#include "ata.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "klog.h"

#define SECTOR_SIZE 512
#define BOOT_RESERVED_SECTORS 512 /* stage2 and the raw kernel live here */

static uint8_t sector_buffer[SECTOR_SIZE];
static installer_fs_t selected_fs = INSTALLER_FS_FAT16;
static uint32_t volume_sectors, reserved_sectors, fat_sectors, root_sectors;
static uint32_t first_data_sector, root_cluster;
static uint8_t sectors_per_cluster;
static uint16_t root_entries;

static void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }

static int write_zero(uint32_t lba) {
    memset(sector_buffer, 0, sizeof(sector_buffer));
    return ata_write_sector(lba, sector_buffer) == 0 ? INSTALLER_OK : INSTALLER_WRITE_ERROR;
}

int installer_detect_disk(void) {
    ata_select_drive(0);
    terminal_writestring("Detecting primary ATA disk...\n");
    if (!ata_is_available() || ata_get_sector_count() == 0) {
        terminal_writestring("ERROR: No usable ATA disk detected\n");
        return INSTALLER_NO_DISK;
    }
    terminal_writestring("Disk found (LBA28)\n");
    return INSTALLER_OK;
}

int installer_prepare_disk(void) {
    /* Do not erase sectors 0..511: the running image's stage2 and kernel are
       intentionally stored there and are described as FAT reserved sectors. */
    ata_select_drive(0);
    terminal_writestring("Preserving AO boot area... OK\n");
    return INSTALLER_OK;
}

static uint32_t fat_bytes_for(installer_fs_t fs, uint32_t clusters) {
    if (fs == INSTALLER_FS_FAT12) return ((clusters + 2) * 3 + 1) / 2;
    if (fs == INSTALLER_FS_FAT16) return (clusters + 2) * 2;
    return (clusters + 2) * 4;
}

static int calculate_layout(installer_fs_t fs) {
    volume_sectors = ata_get_sector_count();
    reserved_sectors = BOOT_RESERVED_SECTORS;
    root_entries = fs == INSTALLER_FS_FAT32 ? 0 : (fs == INSTALLER_FS_FAT12 ? 224 : 512);
    root_sectors = ((uint32_t)root_entries * 32 + 511) / 512;
    sectors_per_cluster = fs == INSTALLER_FS_FAT12 ? 32 : (fs == INSTALLER_FS_FAT16 ? 2 : 1);
    if (volume_sectors <= reserved_sectors + root_sectors + 100) return INSTALLER_UNSUPPORTED;

    fat_sectors = 1;
    for (int i=0; i<16; i++) {
        /* volume_sectors > reserved_sectors + root_sectors + 100 is only
         * guaranteed before this loop starts (the check above). As
         * fat_sectors grows on later iterations, 2*fat_sectors can exceed
         * that 100-sector margin - then this subtraction underflows as
         * uint32_t to a huge value, "clusters" follows, and fat_bytes_for()
         * makes fat_sectors grow even further next iteration instead of
         * converging. Bail out rather than spin into a nonsense layout. */
        if (reserved_sectors + root_sectors + 2 * fat_sectors >= volume_sectors) return INSTALLER_UNSUPPORTED;
        uint32_t data = volume_sectors - reserved_sectors - root_sectors - 2 * fat_sectors;
        uint32_t clusters = data / sectors_per_cluster;
        uint32_t next = (fat_bytes_for(fs, clusters) + 511) / 512;
        if (next == fat_sectors) break;
        fat_sectors = next;
    }
    first_data_sector = reserved_sectors + 2 * fat_sectors + root_sectors;
    /* Same underflow risk as above, now against the converged fat_sectors -
     * without this, a pathological size could still slip through to the
     * subtraction below and masquerade as a huge, wrongly-FAT32-range
     * cluster count, corrupting every subsequent layout calculation. */
    if (first_data_sector >= volume_sectors) return INSTALLER_UNSUPPORTED;
    uint32_t clusters = (volume_sectors - first_data_sector) / sectors_per_cluster;
    if ((fs == INSTALLER_FS_FAT12 && clusters >= 4085) ||
        (fs == INSTALLER_FS_FAT16 && (clusters < 4085 || clusters >= 65525)) ||
        (fs == INSTALLER_FS_FAT32 && clusters < 65525)) return INSTALLER_UNSUPPORTED;
    root_cluster = fs == INSTALLER_FS_FAT32 ? 2 : 0;
    return INSTALLER_OK;
}

static int write_boot_sector(installer_fs_t fs) {
    /* Keep executable bytes from the current AO boot sector, replacing only
       the standard BPB/EBPB window prepared by stage1.asm. */
    if (ata_read_sector(0, sector_buffer) != 0) return INSTALLER_READ_ERROR;
    sector_buffer[0]=0xEB; sector_buffer[1]=0x58; sector_buffer[2]=0x90;
    memcpy(&sector_buffer[3], "AOOS2.0 ", 8);
    put16(&sector_buffer[11], 512); sector_buffer[13]=sectors_per_cluster;
    put16(&sector_buffer[14], (uint16_t)reserved_sectors); sector_buffer[16]=2;
    put16(&sector_buffer[17], root_entries);
    put16(&sector_buffer[19], volume_sectors < 65536 ? (uint16_t)volume_sectors : 0);
    sector_buffer[21]=0xF8;
    put16(&sector_buffer[22], fs == INSTALLER_FS_FAT32 ? 0 : (uint16_t)fat_sectors);
    put16(&sector_buffer[24], 63); put16(&sector_buffer[26], 255); put32(&sector_buffer[28], 0);
    put32(&sector_buffer[32], volume_sectors >= 65536 ? volume_sectors : 0);
    if (fs == INSTALLER_FS_FAT32) {
        put32(&sector_buffer[36], fat_sectors); put16(&sector_buffer[40], 0); put16(&sector_buffer[42], 0);
        put32(&sector_buffer[44], root_cluster); put16(&sector_buffer[48], 256); put16(&sector_buffer[50], 257);
        memset(&sector_buffer[52], 0, 12); sector_buffer[64]=0x80; sector_buffer[66]=0x29;
        put32(&sector_buffer[67], 0xA0200032); memcpy(&sector_buffer[71], "AO OS      ", 11); memcpy(&sector_buffer[82], "FAT32   ", 8);
    } else {
        sector_buffer[36]=0x80; sector_buffer[38]=0x29; put32(&sector_buffer[39], fs == INSTALLER_FS_FAT16 ? 0xA0200016 : 0xA0200012);
        memcpy(&sector_buffer[43], "AO OS      ", 11); memcpy(&sector_buffer[54], fs == INSTALLER_FS_FAT16 ? "FAT16   " : "FAT12   ", 8);
    }
    sector_buffer[510]=0x55; sector_buffer[511]=0xAA;
    if (ata_write_sector(0, sector_buffer) != 0) return INSTALLER_WRITE_ERROR;
    if (fs == INSTALLER_FS_FAT32) {
        memset(sector_buffer, 0, sizeof(sector_buffer));
        put32(&sector_buffer[0], 0x41615252); put32(&sector_buffer[484], 0x61417272);
        put32(&sector_buffer[488], 0xFFFFFFFF); put32(&sector_buffer[492], 0xFFFFFFFF);
        put32(&sector_buffer[508], 0xAA550000);
        if (ata_write_sector(256, sector_buffer) != 0) return INSTALLER_WRITE_ERROR;
        if (ata_read_sector(0, sector_buffer) != 0 || ata_write_sector(257, sector_buffer) != 0) return INSTALLER_WRITE_ERROR;
        if (ata_read_sector(256, sector_buffer) != 0 || ata_write_sector(258, sector_buffer) != 0) return INSTALLER_WRITE_ERROR;
    }
    return INSTALLER_OK;
}

static int set_fat_entry_in_sector(installer_fs_t fs, uint8_t* sec, uint32_t cluster, uint32_t value) {
    uint32_t offset = fs == INSTALLER_FS_FAT12 ? cluster + cluster/2 : (fs == INSTALLER_FS_FAT16 ? cluster*2 : cluster*4);
    if (offset >= 512 || (fs == INSTALLER_FS_FAT12 && offset == 511)) return -1;
    if (fs == INSTALLER_FS_FAT12) {
        uint16_t old = sec[offset] | ((uint16_t)sec[offset+1]<<8);
        old = cluster&1 ? (old&0x000F)|(value<<4) : (old&0xF000)|(value&0xFFF);
        put16(sec+offset, old);
    } else if (fs == INSTALLER_FS_FAT16) put16(sec+offset, (uint16_t)value);
    else put32(sec+offset, value);
    return 0;
}

static int initialize_fats(installer_fs_t fs) {
    uint8_t first[512]; memset(first, 0, sizeof(first));
    if (fs == INSTALLER_FS_FAT12) { first[0]=0xF8; first[1]=0xFF; first[2]=0xFF; }
    else if (fs == INSTALLER_FS_FAT16) { put16(first, 0xFFF8); put16(first+2, 0xFFFF); }
    else { put32(first, 0x0FFFFFF8); put32(first+4, 0xFFFFFFFF); put32(first+8, 0x0FFFFFFF); }
    for (uint32_t copy=0; copy<2; copy++) {
        uint32_t base=reserved_sectors+copy*fat_sectors;
        for (uint32_t s=0; s<fat_sectors; s++) {
            if (ata_write_sector(base+s, s==0 ? first : sector_buffer) != 0) return INSTALLER_WRITE_ERROR;
            if (s==0) memset(sector_buffer, 0, sizeof(sector_buffer));
        }
    }
    return INSTALLER_OK;
}

int installer_format_filesystem_as(installer_fs_t fs) {
    ata_select_drive(0); selected_fs=fs;
    int result=calculate_layout(fs); if (result != INSTALLER_OK) { terminal_writestring("ERROR: Disk is too small for selected FAT type\n"); return result; }
    terminal_writestring("Writing "); terminal_writestring(fs==INSTALLER_FS_FAT12?"FAT12":(fs==INSTALLER_FS_FAT16?"FAT16":"FAT32")); terminal_writestring(" filesystem...\n");
    if ((result=write_boot_sector(fs)) != INSTALLER_OK) return result;
    memset(sector_buffer, 0, sizeof(sector_buffer));
    if ((result=initialize_fats(fs)) != INSTALLER_OK) return result;
    for (uint32_t s=0; s<root_sectors; s++) if (write_zero(reserved_sectors+2*fat_sectors+s)!=INSTALLER_OK) return INSTALLER_WRITE_ERROR;
    if (fs==INSTALLER_FS_FAT32) for (uint32_t s=0;s<sectors_per_cluster;s++) if (write_zero(first_data_sector+s)!=INSTALLER_OK) return INSTALLER_WRITE_ERROR;
    terminal_writestring("Filesystem formatted successfully.\n"); return INSTALLER_OK;
}

int installer_format_filesystem(void) { return installer_format_filesystem_as(selected_fs); }

static void make_entry(uint8_t* e, const char* base, const char* ext, uint32_t cluster, uint32_t size) {
    memset(e, 0, 32); memset(e, ' ', 11); memcpy(e, base, strlen(base)); memcpy(e+8, ext, strlen(ext)); e[11]=0x20;
    put16(e+26, (uint16_t)cluster); if (selected_fs==INSTALLER_FS_FAT32) put16(e+20, (uint16_t)(cluster>>16)); put32(e+28, size);
}

static void make_volume_entry(uint8_t* e) {
    memset(e, 0, 32); memcpy(e, "AO OS      ", 11); e[11] = 0x08;
}

static int installer_create_welcome_files(void) {
    const char* welcome="Welcome to AO OS!\r\n\r\nThe disk is formatted and ready.\r\nTry: ls, cat README.TXT, edit test.txt\r\n";
    const char* readme="AO OS persistent storage\r\nSupported formats: FAT12, FAT16 and FAT32.\r\n";
    uint32_t c1=selected_fs==INSTALLER_FS_FAT32?3:2, c2=c1+1;
    uint32_t root_sector=selected_fs==INSTALLER_FS_FAT32?first_data_sector:reserved_sectors+2*fat_sectors;
    if (ata_read_sector(root_sector, sector_buffer)!=0) return INSTALLER_READ_ERROR;
    make_volume_entry(sector_buffer);
    make_entry(sector_buffer+32,"WELCOME","TXT",c1,strlen(welcome));
    make_entry(sector_buffer+64,"README","TXT",c2,strlen(readme));
    if (ata_write_sector(root_sector,sector_buffer)!=0) return INSTALLER_WRITE_ERROR;
    memset(sector_buffer,0,512); memcpy(sector_buffer,welcome,strlen(welcome)); if (ata_write_sector(first_data_sector+(c1-2)*sectors_per_cluster,sector_buffer)!=0) return INSTALLER_WRITE_ERROR;
    memset(sector_buffer,0,512); memcpy(sector_buffer,readme,strlen(readme)); if (ata_write_sector(first_data_sector+(c2-2)*sectors_per_cluster,sector_buffer)!=0) return INSTALLER_WRITE_ERROR;
    for (uint32_t copy=0;copy<2;copy++) {
        uint32_t base=reserved_sectors+copy*fat_sectors; if (ata_read_sector(base,sector_buffer)!=0) return INSTALLER_READ_ERROR;
        uint32_t eoc=selected_fs==INSTALLER_FS_FAT12?0xFFF:(selected_fs==INSTALLER_FS_FAT16?0xFFFF:0x0FFFFFFF);
        set_fat_entry_in_sector(selected_fs,sector_buffer,c1,eoc); set_fat_entry_in_sector(selected_fs,sector_buffer,c2,eoc);
        if (ata_write_sector(base,sector_buffer)!=0) return INSTALLER_WRITE_ERROR;
    }
    terminal_writestring("Welcome files created.\n"); return INSTALLER_OK;
}

int installer_install_kernel(void) { terminal_writestring("Kernel preserved in reserved area.\n"); return INSTALLER_OK; }
int installer_install_bootloader(void) { terminal_writestring("Bootloader preserved and BPB installed.\n"); return INSTALLER_OK; }

static installer_fs_t choose_filesystem(void) {
    terminal_writestring("\nChoose filesystem:\n  1. FAT12 (compatibility)\n  2. FAT16 (recommended)\n  3. FAT32 (large volumes)\nSelection [1-3]: ");
    for (;;) { unsigned char c=keyboard_getchar(); if (c>='1'&&c<='3') { terminal_putchar(c); terminal_writestring("\n"); return c=='1'?INSTALLER_FS_FAT12:(c=='2'?INSTALLER_FS_FAT16:INSTALLER_FS_FAT32); } asm volatile("hlt"); }
}

int installer_run(void) {
    terminal_writestring("\n================ AO OS Installer ================\n");
    if (installer_detect_disk()!=INSTALLER_OK) return INSTALLER_NO_DISK;
    selected_fs=choose_filesystem();
    if (installer_prepare_disk()!=INSTALLER_OK) { klog_error("Installer disk preparation failed"); return INSTALLER_WRITE_ERROR; }
    if (installer_format_filesystem()!=INSTALLER_OK) { klog_error("Installer filesystem format failed"); return INSTALLER_WRITE_ERROR; }
    if (installer_create_welcome_files()!=INSTALLER_OK) { klog_error("Installer welcome file creation failed"); return INSTALLER_WRITE_ERROR; }
    installer_install_kernel(); installer_install_bootloader();
    terminal_writestring("Installation complete. Rebooting in 3 seconds...\n"); klog_info("Installation completed successfully");
    extern void timer_wait(uint32_t); extern void system_reboot(void); timer_wait(300); system_reboot(); return INSTALLER_OK;
}
