#include "pci.h"
#include "io.h"
#include "string.h"
#include "klog.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_read_device(uint8_t bus, uint8_t slot, uint8_t function) {
    if (device_count >= PCI_MAX_DEVICES) return;

    uint32_t id = pci_config_read32(bus, slot, function, 0x00);
    uint16_t vendor = (uint16_t)(id & 0xFFFF);
    if (vendor == 0xFFFF) return;

    pci_device_t* d = &devices[device_count++];
    memset(d, 0, sizeof(*d));
    d->bus = bus;
    d->slot = slot;
    d->function = function;
    d->vendor_id = vendor;
    d->device_id = (uint16_t)(id >> 16);

    uint32_t classreg = pci_config_read32(bus, slot, function, 0x08);
    d->revision = (uint8_t)(classreg & 0xFF);
    d->prog_if = (uint8_t)((classreg >> 8) & 0xFF);
    d->subclass = (uint8_t)((classreg >> 16) & 0xFF);
    d->class_code = (uint8_t)((classreg >> 24) & 0xFF);
    d->interrupt_line = (uint8_t)(pci_config_read32(bus, slot, function, 0x3C) & 0xFF);

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_config_read32(bus, slot, function, (uint8_t)(0x10 + i * 4));
    }
}

void pci_init(void) {
    device_count = 0;
    memset(devices, 0, sizeof(devices));

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_config_read32((uint8_t)bus, slot, 0, 0x00);
            if ((uint16_t)(id & 0xFFFF) == 0xFFFF) continue;

            uint32_t header = pci_config_read32((uint8_t)bus, slot, 0, 0x0C);
            uint8_t multi = (uint8_t)((header >> 16) & 0x80);
            uint8_t max_fn = multi ? 8 : 1;
            for (uint8_t fn = 0; fn < max_fn; fn++) {
                pci_read_device((uint8_t)bus, slot, fn);
            }
        }
    }

    klog_info("PCI bus scan complete");
}

int pci_get_devices(pci_device_t** out) {
    *out = devices;
    return device_count;
}

const pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass) return &devices[i];
    }
    return 0;
}
