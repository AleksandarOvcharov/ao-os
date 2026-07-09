#include "net.h"
#include "pci.h"
#include "klog.h"

static net_status_t status;

const char* net_driver_name(net_driver_t driver) {
    switch (driver) {
        case NET_DRIVER_NE2000: return "NE2000";
        case NET_DRIVER_VIRTIO: return "virtio-net";
        default: return "none";
    }
}

static int is_ne2000(uint16_t vendor, uint16_t device) {
    return vendor == 0x10EC && device == 0x8029;
}

static int is_virtio_net(uint16_t vendor, uint16_t device) {
    return vendor == 0x1AF4 && device >= 0x1000 && device <= 0x104F;
}

void net_init(void) {
    status.initialized = 1;
    status.driver = NET_DRIVER_NONE;
    status.packets_rx = 0;
    status.packets_tx = 0;

    pci_device_t* devices = 0;
    int count = pci_get_devices(&devices);
    for (int i = 0; i < count; i++) {
        if (devices[i].class_code != 0x02 || devices[i].subclass != 0x00) continue;

        status.vendor_id = devices[i].vendor_id;
        status.device_id = devices[i].device_id;
        status.bus = devices[i].bus;
        status.slot = devices[i].slot;
        status.function = devices[i].function;

        if (is_ne2000(devices[i].vendor_id, devices[i].device_id)) {
            status.driver = NET_DRIVER_NE2000;
            klog_info("Network device detected: NE2000");
            return;
        }
        if (is_virtio_net(devices[i].vendor_id, devices[i].device_id)) {
            status.driver = NET_DRIVER_VIRTIO;
            klog_info("Network device detected: virtio-net");
            return;
        }
    }

    klog_warn("No supported network device detected");
}

const net_status_t* net_get_status(void) {
    return &status;
}

int net_arp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_ipv4_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_icmp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_udp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_tcp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
