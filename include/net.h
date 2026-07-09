#ifndef NET_H
#define NET_H

#include <stdint.h>

typedef enum {
    NET_DRIVER_NONE = 0,
    NET_DRIVER_NE2000,
    NET_DRIVER_VIRTIO
} net_driver_t;

typedef struct {
    int initialized;
    net_driver_t driver;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint32_t packets_rx;
    uint32_t packets_tx;
} net_status_t;

void net_init(void);
const net_status_t* net_get_status(void);
const char* net_driver_name(net_driver_t driver);
int net_arp_init(void);
int net_ipv4_init(void);
int net_icmp_init(void);
int net_udp_init(void);
int net_tcp_init(void);

#endif
