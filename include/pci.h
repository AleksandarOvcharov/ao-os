#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 32

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint32_t bar[6];
} pci_device_t;

void pci_init(void);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
int pci_get_devices(pci_device_t** devices);
const pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass);

#endif
