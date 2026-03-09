#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6

#define ATA_REG_DATA        0x1F0
#define ATA_REG_ERROR       0x1F1
#define ATA_REG_FEATURES    0x1F1
#define ATA_REG_SECCOUNT    0x1F2
#define ATA_REG_LBA_LOW     0x1F3
#define ATA_REG_LBA_MID     0x1F4
#define ATA_REG_LBA_HIGH    0x1F5
#define ATA_REG_DRIVE       0x1F6
#define ATA_REG_STATUS      0x1F7
#define ATA_REG_COMMAND     0x1F7

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC

#define ATA_STATUS_BSY      0x80
#define ATA_STATUS_DRDY     0x40
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_ERR      0x01

void ata_init(void);
void ata_wait_bsy(void);
void ata_wait_drq(void);
void ata_select_drive(int slave);
int ata_read_sector(uint32_t lba, uint8_t* buffer);
int ata_write_sector(uint32_t lba, uint8_t* buffer);
int ata_is_available(void);

#endif
