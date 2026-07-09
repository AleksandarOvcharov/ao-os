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
    uint32_t rx_errors;
    uint32_t arp_entries;
    uint32_t last_ping_ip;
    int last_ping_ok;
    uint32_t io_base;
    uint8_t mac[6];
} net_status_t;

void net_init(void);
const net_status_t* net_get_status(void);
const char* net_driver_name(net_driver_t driver);
int net_arp_init(void);
int net_ipv4_init(void);
int net_icmp_init(void);
int net_udp_init(void);
int net_tcp_init(void);
int net_send_packet(const uint8_t* data, uint16_t len);
int net_receive_packet(uint8_t* data, uint16_t* len);
int net_poll(void);
int net_ping_ipv4(uint32_t ip);
uint32_t net_parse_ipv4(const char* s);
int net_format_arp_cache(char* buffer, uint32_t* size, uint32_t max_size);
int net_format_device_stats(char* buffer, uint32_t* size, uint32_t max_size);

#endif
