#include "ata.h"
#include "io.h"
#include "klog.h"

static int ata_available = 0;
static uint32_t ata_sector_count = 0;
static uint8_t ata_drive_select = 0xE0; /* 0xE0=master, 0xF0=slave */

#define ATA_TIMEOUT 1000000

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

    /* IDENTIFY gives the formatter the actual LBA28 capacity. */
    outb(ATA_REG_DRIVE, 0xA0);
    outb(ATA_REG_SECCOUNT, 0);
    outb(ATA_REG_LBA_LOW, 0);
    outb(ATA_REG_LBA_MID, 0);
    outb(ATA_REG_LBA_HIGH, 0);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    if (ata_wait_bsy() == 0 && ata_wait_drq() == 0) {
        for (int i = 0; i < 256; i++) {
            uint16_t word = inw(ATA_REG_DATA);
            if (i == 60) ata_sector_count = word;
            if (i == 61) ata_sector_count |= (uint32_t)word << 16;
        }
    }
    ata_available = 1;
    klog_info("ATA driver initialized");
}

int ata_is_available(void) {
    return ata_available;
}

uint32_t ata_get_sector_count(void) { return ata_sector_count; }

void ata_select_drive(int slave) {
    ata_drive_select = slave ? 0xF0 : 0xE0;
}

/* ata_read_sector/ata_write_sector are each one stateful, multi-step
 * exchange with a single physical controller (select drive, program LBA,
 * issue command, wait, transfer 256 words) - there's no per-transaction
 * identifier, so two interleaved transactions would scramble into one
 * another's bytes. They're reachable from preemptible contexts now
 * (SYS_OPEN's fs_read() runs with interrupts enabled - see syscall_handler
 * in interrupt.asm), so a timer tick landing mid-transaction could switch
 * to a different process that starts its own disk I/O before this one
 * finishes. cli/sti makes each call atomic with respect to the scheduler,
 * the same technique used in process.c for its own cross-tick hazards.
 * This never risks a GETCHAR-style deadlock: unlike waiting on a keypress,
 * ata_wait_bsy()/ata_wait_drq() poll a hardware status port that updates
 * on its own regardless of whether interrupts are enabled. */

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    asm volatile("cli");
    int result = -1;

    if (ata_wait_bsy() != 0) goto done;

    outb(ATA_REG_DRIVE, ata_drive_select | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_bsy() != 0) goto done;

    if (inb(ATA_REG_STATUS) & ATA_STATUS_ERR) {
        klog_error("ATA read error");
        goto done;
    }

    if (ata_wait_drq() != 0) goto done;

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_REG_DATA);
        buffer[i * 2] = (uint8_t)data;
        buffer[i * 2 + 1] = (uint8_t)(data >> 8);
    }

    result = 0;
done:
    asm volatile("sti");
    return result;
}

int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    asm volatile("cli");
    int result = -1;

    if (ata_wait_bsy() != 0) goto done;

    outb(ATA_REG_DRIVE, ata_drive_select | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_bsy() != 0) goto done;
    if (ata_wait_drq() != 0) goto done;

    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_REG_DATA, data);
    }

    outb(ATA_REG_COMMAND, 0xE7);
    if (ata_wait_bsy() != 0) goto done;

    result = 0;
done:
    asm volatile("sti");
    return result;
}
