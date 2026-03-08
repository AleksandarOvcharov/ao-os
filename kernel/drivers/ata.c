#include "ata.h"
#include "io.h"
#include "klog.h"

static int ata_available = 0;

void ata_wait_bsy(void) {
    while (inb(ATA_REG_STATUS) & ATA_STATUS_BSY);
}

void ata_wait_drq(void) {
    while (!(inb(ATA_REG_STATUS) & ATA_STATUS_DRQ));
}

void ata_init(void) {
    klog_info("Initializing ATA driver...");
    
    outb(ATA_REG_DRIVE, 0xA0);
    
    ata_wait_bsy();
    
    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0xFF) {
        klog_warn("No ATA drive detected");
        ata_available = 0;
        return;
    }
    
    ata_available = 1;
    klog_info("ATA driver initialized");
}

int ata_is_available(void) {
    return ata_available;
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    
    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    ata_wait_bsy();
    
    uint8_t status = inb(ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) {
        klog_error("ATA read error");
        return -1;
    }
    
    ata_wait_drq();
    
    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_REG_DATA);
        buffer[i * 2] = (uint8_t)data;
        buffer[i * 2 + 1] = (uint8_t)(data >> 8);
    }
    
    return 0;
}

int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    
    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    ata_wait_bsy();
    ata_wait_drq();
    
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_REG_DATA, data);
    }
    
    outb(ATA_REG_COMMAND, 0xE7);
    ata_wait_bsy();
    
    return 0;
}
