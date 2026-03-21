#include "ata.h"
#include "io.h"
#include "klog.h"

static int ata_available = 0;
static uint8_t ata_drive_select = 0xE0; /* 0xE0=master, 0xF0=slave */

#define ATA_TIMEOUT 100000

static int ata_wait_bsy(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        if (!(inb(ATA_REG_STATUS) & ATA_STATUS_BSY)) return 0;
    }
    return -1;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        if (inb(ATA_REG_STATUS) & ATA_STATUS_DRQ) return 0;
    }
    return -1;
}

void ata_init(void) {
    klog_info("Initializing ATA driver...");

    outb(ATA_REG_DRIVE, 0xA0);

    if (ata_wait_bsy() != 0) {
        klog_warn("ATA drive not responding (timeout)");
        ata_available = 0;
        return;
    }

    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0xFF || status == 0x00) {
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

void ata_select_drive(int slave) {
    ata_drive_select = slave ? 0xF0 : 0xE0;
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(ATA_REG_DRIVE, ata_drive_select | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_bsy() != 0) return -1;

    uint8_t status = inb(ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) {
        klog_error("ATA read error");
        return -1;
    }

    if (ata_wait_drq() != 0) return -1;

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_REG_DATA);
        buffer[i * 2] = (uint8_t)data;
        buffer[i * 2 + 1] = (uint8_t)(data >> 8);
    }

    return 0;
}

int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(ATA_REG_DRIVE, ata_drive_select | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_bsy() != 0) return -1;
    if (ata_wait_drq() != 0) return -1;

    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_REG_DATA, data);
    }

    outb(ATA_REG_COMMAND, 0xE7);
    if (ata_wait_bsy() != 0) return -1;

    return 0;
}
