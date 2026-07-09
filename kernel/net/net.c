#include "net.h"
#include "pci.h"
#include "io.h"
#include "string.h"
#include "klog.h"
#include "idt.h"
#include "timer.h"

#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800
#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP  17

#define NE_CR     0x00
#define NE_PSTART 0x01
#define NE_PSTOP  0x02
#define NE_BNRY   0x03
#define NE_TPSR   0x04
#define NE_TBCR0  0x05
#define NE_ISR    0x07
#define NE_RSAR0  0x08
#define NE_RBCR0  0x0A
#define NE_RCR    0x0C
#define NE_TCR    0x0D
#define NE_DCR    0x0E
#define NE_IMR    0x0F
#define NE_DATA   0x10
#define NE_CURR   0x07

#define NE_TX_PAGE    0x40
#define NE_RX_START   0x46
#define NE_RX_STOP    0x80
#define LOCAL_IP      0x0A00020FULL

static net_status_t status;

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint64_t updated;
    int valid;
} arp_entry_t;

typedef struct {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    char data[128];
    int valid;
} udp_packet_t;

static arp_entry_t arp_cache[8];
static uint16_t icmp_seq;
static uint8_t rx_boundary;
static uint32_t pending_ping_ip;
static uint16_t pending_ping_seq;
static udp_packet_t last_udp;

#define ARP_TTL_TICKS 6000
#define PING_WAIT_SPINS 120000

static uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint32_t bswap32(uint32_t v) {
    return ((v & 0x000000FFU) << 24) | ((v & 0x0000FF00U) << 8) |
           ((v & 0x00FF0000U) >> 8) | ((v & 0xFF000000U) >> 24);
}

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

static uint16_t checksum(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint16_t)((p[0] << 8) | p[1]);
        p += 2;
        len -= 2;
    }
    if (len) sum += (uint16_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static void append_str(char* b, uint32_t* p, uint32_t max, const char* s) {
    while (*s && *p + 1 < max) b[(*p)++] = *s++;
}

static void append_u32(char* b, uint32_t* p, uint32_t max, uint32_t v) {
    char tmp[16]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0 && *p + 1 < max) b[(*p)++] = tmp[--n];
}

static void append_hex2(char* b, uint32_t* p, uint32_t max, uint8_t v) {
    const char* h = "0123456789abcdef";
    if (*p + 2 >= max) return;
    b[(*p)++] = h[v >> 4];
    b[(*p)++] = h[v & 0x0F];
}

static void append_ip(char* b, uint32_t* p, uint32_t max, uint32_t ip) {
    append_u32(b, p, max, (ip >> 24) & 0xFF); append_str(b, p, max, ".");
    append_u32(b, p, max, (ip >> 16) & 0xFF); append_str(b, p, max, ".");
    append_u32(b, p, max, (ip >> 8) & 0xFF); append_str(b, p, max, ".");
    append_u32(b, p, max, ip & 0xFF);
}

static void append_mac(char* b, uint32_t* p, uint32_t max, const uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        if (i) append_str(b, p, max, ":");
        append_hex2(b, p, max, mac[i]);
    }
}

static void expire_arp(void) {
    uint64_t now = timer_get_ticks();
    status.arp_entries = 0;
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid && now - arp_cache[i].updated > ARP_TTL_TICKS) arp_cache[i].valid = 0;
        if (arp_cache[i].valid) status.arp_entries++;
    }
}

static uint16_t io_base_from_bar(uint32_t bar) {
    return (uint16_t)(bar & ~0x3U);
}

static void ne2k_init(uint16_t io) {
    outb(io + NE_CR, 0x21);
    outb(io + NE_DCR, 0x49);
    outb(io + NE_RBCR0, 0);
    outb(io + NE_RBCR0 + 1, 0);
    outb(io + NE_RCR, 0x20);
    outb(io + NE_TCR, 0x02);
    rx_boundary = NE_RX_START;
    outb(io + NE_PSTART, NE_RX_START);
    outb(io + NE_PSTOP, NE_RX_STOP);
    outb(io + NE_BNRY, rx_boundary);
    outb(io + NE_ISR, 0xFF);
    outb(io + NE_IMR, 0x1F);
    outb(io + NE_CR, 0x61);
    outb(io + NE_CURR, NE_RX_START + 1);
    for (int i = 0; i < 6; i++) outb(io + 0x01 + i, status.mac[i]);
    for (int i = 0; i < 8; i++) outb(io + 0x08 + i, 0);
    outb(io + NE_CR, 0x22);
    outb(io + NE_TCR, 0x00);
    outb(io + NE_RCR, 0x04);
}

void net_init(void) {
    memset(&status, 0, sizeof(status));
    memset(arp_cache, 0, sizeof(arp_cache));
    status.initialized = 1;
    status.driver = NET_DRIVER_NONE;
    status.mac[0]=0x52; status.mac[1]=0x54; status.mac[2]=0x00;
    status.mac[3]=0x12; status.mac[4]=0x34; status.mac[5]=0x56;

    pci_device_t* devices = 0;
    int count = pci_get_devices(&devices);
    for (int i = 0; i < count; i++) {
        if (devices[i].class_code != 0x02 || devices[i].subclass != 0x00) continue;

        status.vendor_id = devices[i].vendor_id;
        status.device_id = devices[i].device_id;
        status.bus = devices[i].bus;
        status.slot = devices[i].slot;
        status.function = devices[i].function;
        status.irq = devices[i].interrupt_line;

        if (is_ne2000(devices[i].vendor_id, devices[i].device_id)) {
            status.driver = NET_DRIVER_NE2000;
            status.io_base = io_base_from_bar(devices[i].bar[0]);
            ne2k_init((uint16_t)status.io_base);
            if (status.irq == 10 || status.irq == 11) idt_unmask_irq(status.irq);
            klog_info("Network device initialized: NE2000");
            return;
        }
        if (is_virtio_net(devices[i].vendor_id, devices[i].device_id)) {
            status.driver = NET_DRIVER_VIRTIO;
            status.io_base = io_base_from_bar(devices[i].bar[0]);
            if (status.irq == 10 || status.irq == 11) idt_unmask_irq(status.irq);
            klog_info("Network device detected: virtio-net");
            return;
        }
    }

    klog_warn("No supported network device detected");
}

const net_status_t* net_get_status(void) { return &status; }
int net_arp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_ipv4_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_icmp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_udp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }
int net_tcp_init(void) { return status.driver != NET_DRIVER_NONE ? 0 : -1; }

void net_irq_handler(uint64_t irq) {
    if (status.driver != NET_DRIVER_NE2000 || irq != status.irq || !status.io_base) return;
    uint16_t io = (uint16_t)status.io_base;
    uint8_t isr = inb(io + NE_ISR);
    if (!isr) return;
    outb(io + NE_ISR, isr);
    status.irq_count++;
    if (isr & 0x15) net_poll();
    if (isr & 0x10) status.rx_errors++;
}

static void arp_cache_put(uint32_t ip, const uint8_t* mac) {
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) { slot = i; break; }
        if (!arp_cache[i].valid && slot < 0) slot = i;
    }
    if (slot < 0) slot = 0;
    arp_cache[slot].ip = ip;
    memcpy(arp_cache[slot].mac, mac, 6);
    arp_cache[slot].updated = timer_get_ticks();
    arp_cache[slot].valid = 1;
    status.arp_entries = 0;
    for (int i = 0; i < 8; i++) if (arp_cache[i].valid) status.arp_entries++;
}

int net_send_packet(const uint8_t* data, uint16_t len) {
    if (!data || len == 0 || status.driver != NET_DRIVER_NE2000 || !status.io_base) return -1;
    uint8_t frame[1536];
    uint16_t out_len = len < 60 ? 60 : len;
    memset(frame, 0, sizeof(frame));
    memcpy(frame, data, len);

    uint16_t io = (uint16_t)status.io_base;
    outb(io + NE_ISR, 0x40);
    outb(io + NE_RBCR0, (uint8_t)(out_len & 0xFF));
    outb(io + NE_RBCR0 + 1, (uint8_t)(out_len >> 8));
    outb(io + NE_RSAR0, 0x00);
    outb(io + NE_RSAR0 + 1, 0x40);
    outb(io + NE_CR, 0x12);
    outsw(io + NE_DATA, frame, (out_len + 1) / 2);
    outb(io + NE_TPSR, 0x40);
    outb(io + NE_TBCR0, (uint8_t)(out_len & 0xFF));
    outb(io + NE_TBCR0 + 1, (uint8_t)(out_len >> 8));
    outb(io + NE_CR, 0x26);
    status.packets_tx++;
    return 0;
}

int net_receive_packet(uint8_t* data, uint16_t* len) {
    if (!data || !len || status.driver != NET_DRIVER_NE2000 || !status.io_base) return -1;
    uint16_t io = (uint16_t)status.io_base;

    outb(io + NE_CR, 0x62);
    uint8_t curr = inb(io + NE_CURR);
    outb(io + NE_CR, 0x22);

    uint8_t packet_page = (uint8_t)(rx_boundary + 1);
    if (packet_page >= NE_RX_STOP) packet_page = NE_RX_START;
    if (packet_page == curr) { *len = 0; return 0; }

    uint8_t hdr[4];
    uint16_t addr = (uint16_t)packet_page << 8;
    outb(io + NE_RBCR0, 4);
    outb(io + NE_RBCR0 + 1, 0);
    outb(io + NE_RSAR0, (uint8_t)(addr & 0xFF));
    outb(io + NE_RSAR0 + 1, (uint8_t)(addr >> 8));
    outb(io + NE_CR, 0x0A);
    insw(io + NE_DATA, hdr, 2);

    uint8_t next_page = hdr[1];
    uint16_t count = (uint16_t)hdr[2] | ((uint16_t)hdr[3] << 8);
    if (next_page < NE_RX_START || next_page >= NE_RX_STOP || count < 4 || count > 1600) {
        status.rx_errors++;
        rx_boundary = curr == NE_RX_START ? (NE_RX_STOP - 1) : (uint8_t)(curr - 1);
        outb(io + NE_BNRY, rx_boundary);
        *len = 0;
        return -1;
    }

    uint16_t payload_len = count - 4;
    uint16_t first = (uint16_t)((NE_RX_STOP - packet_page) << 8);
    if (first > count) first = count;
    uint16_t first_payload = first > 4 ? (uint16_t)(first - 4) : 0;
    if (first_payload > payload_len) first_payload = payload_len;

    outb(io + NE_RBCR0, (uint8_t)(first_payload & 0xFF));
    outb(io + NE_RBCR0 + 1, (uint8_t)(first_payload >> 8));
    outb(io + NE_RSAR0, (uint8_t)((addr + 4) & 0xFF));
    outb(io + NE_RSAR0 + 1, (uint8_t)((addr + 4) >> 8));
    outb(io + NE_CR, 0x0A);
    insw(io + NE_DATA, data, (first_payload + 1) / 2);

    if (first_payload < payload_len) {
        uint16_t rest = payload_len - first_payload;
        outb(io + NE_RBCR0, (uint8_t)(rest & 0xFF));
        outb(io + NE_RBCR0 + 1, (uint8_t)(rest >> 8));
        outb(io + NE_RSAR0, 0);
        outb(io + NE_RSAR0 + 1, NE_RX_START);
        outb(io + NE_CR, 0x0A);
        insw(io + NE_DATA, data + first_payload, (rest + 1) / 2);
    }

    rx_boundary = next_page == NE_RX_START ? (NE_RX_STOP - 1) : (uint8_t)(next_page - 1);
    outb(io + NE_BNRY, rx_boundary);
    outb(io + NE_ISR, 0x01);
    *len = payload_len;
    status.packets_rx++;
    return 1;
}

static arp_entry_t* arp_lookup(uint32_t ip) {
    expire_arp();
    for (int i = 0; i < 8; i++) if (arp_cache[i].valid && arp_cache[i].ip == ip) return &arp_cache[i];
    return 0;
}

static void send_arp_reply(const uint8_t* request) {
    uint8_t p[42];
    memcpy(p, request + 6, 6);
    memcpy(p + 6, status.mac, 6);
    *(uint16_t*)(p + 12) = bswap16(ETH_TYPE_ARP);
    *(uint16_t*)(p + 14) = bswap16(1);
    *(uint16_t*)(p + 16) = bswap16(ETH_TYPE_IPV4);
    p[18] = 6; p[19] = 4;
    *(uint16_t*)(p + 20) = bswap16(2);
    memcpy(p + 22, status.mac, 6);
    *(uint32_t*)(p + 28) = bswap32(LOCAL_IP);
    memcpy(p + 32, request + 22, 6);
    memcpy(p + 38, request + 28, 4);
    net_send_packet(p, sizeof(p));
}

static int send_arp_request(uint32_t target_ip) {
    uint8_t p[42];
    memset(p, 0, sizeof(p));
    for (int i = 0; i < 6; i++) p[i] = 0xFF;
    memcpy(p + 6, status.mac, 6);
    *(uint16_t*)(p + 12) = bswap16(ETH_TYPE_ARP);
    *(uint16_t*)(p + 14) = bswap16(1);
    *(uint16_t*)(p + 16) = bswap16(ETH_TYPE_IPV4);
    p[18] = 6; p[19] = 4;
    *(uint16_t*)(p + 20) = bswap16(1);
    memcpy(p + 22, status.mac, 6);
    *(uint32_t*)(p + 28) = bswap32(LOCAL_IP);
    *(uint32_t*)(p + 38) = bswap32(target_ip);
    return net_send_packet(p, sizeof(p));
}

int net_ping_ipv4(uint32_t ip) {
    if (status.driver == NET_DRIVER_NONE) return -1;
    net_poll();
    arp_entry_t* e = arp_lookup(ip);
    if (!e) {
        status.last_ping_ip = ip;
        status.last_ping_ok = 0;
        for (int attempt = 0; attempt < 3 && !e; attempt++) {
            if (send_arp_request(ip) != 0) return -1;
            for (uint32_t spin = 0; spin < PING_WAIT_SPINS; spin++) {
                net_poll();
                e = arp_lookup(ip);
                if (e) break;
            }
        }
        if (!e) return 0;
    }

    uint8_t p[14 + 20 + 8 + 16];
    memset(p, 0, sizeof(p));
    memcpy(p, e->mac, 6);
    memcpy(p + 6, status.mac, 6);
    *(uint16_t*)(p + 12) = bswap16(ETH_TYPE_IPV4);
    uint8_t* iphdr = p + 14;
    iphdr[0] = 0x45;
    *(uint16_t*)(iphdr + 2) = bswap16(20 + 8 + 16);
    iphdr[8] = 64;
    iphdr[9] = IP_PROTO_ICMP;
    *(uint32_t*)(iphdr + 12) = bswap32(LOCAL_IP);
    *(uint32_t*)(iphdr + 16) = bswap32(ip);
    *(uint16_t*)(iphdr + 10) = checksum(iphdr, 20);
    uint8_t* icmp = p + 14 + 20;
    icmp[0] = 8;
    *(uint16_t*)(icmp + 4) = bswap16(1);
    pending_ping_ip = ip;
    pending_ping_seq = ++icmp_seq;
    status.last_ping_ip = ip;
    status.last_ping_ok = 0;
    status.last_ping_seq = pending_ping_seq;
    *(uint16_t*)(icmp + 6) = bswap16(pending_ping_seq);
    memcpy(icmp + 8, "AO-OS-PING-0000", 16);
    *(uint16_t*)(icmp + 2) = checksum(icmp, 8 + 16);
    if (net_send_packet(p, sizeof(p)) != 0) return -1;
    for (uint32_t spin = 0; spin < PING_WAIT_SPINS * 3; spin++) {
        net_poll();
        if (status.last_ping_ok) return 1;
    }
    return 0;
}

static void send_icmp_echo_reply(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 20 + 8) return;
    uint8_t reply[1514];
    uint16_t ip_len = bswap16(*(const uint16_t*)(frame + 16));
    if ((uint32_t)ip_len + 14 > len || (uint32_t)ip_len + 14 > sizeof(reply)) return;
    memcpy(reply, frame, ip_len + 14);
    memcpy(reply, frame + 6, 6);
    memcpy(reply + 6, status.mac, 6);
    uint8_t* ip = reply + 14;
    uint32_t src = bswap32(*(uint32_t*)(ip + 12));
    *(uint32_t*)(ip + 12) = bswap32(LOCAL_IP);
    *(uint32_t*)(ip + 16) = bswap32(src);
    *(uint16_t*)(ip + 10) = 0;
    *(uint16_t*)(ip + 10) = checksum(ip, (uint32_t)((ip[0] & 0x0F) * 4));
    uint8_t* icmp = ip + ((ip[0] & 0x0F) * 4);
    icmp[0] = 0;
    icmp[2] = 0; icmp[3] = 0;
    *(uint16_t*)(icmp + 2) = checksum(icmp, ip_len - (uint16_t)((ip[0] & 0x0F) * 4));
    net_send_packet(reply, (uint16_t)(ip_len + 14));
}

static void handle_arp(const uint8_t* frame, uint16_t len) {
    if (len < 42) return;
    uint16_t op = bswap16(*(const uint16_t*)(frame + 20));
    uint32_t spa = bswap32(*(const uint32_t*)(frame + 28));
    uint32_t tpa = bswap32(*(const uint32_t*)(frame + 38));
    arp_cache_put(spa, frame + 22);
    if (op == 1 && tpa == LOCAL_IP) send_arp_reply(frame);
}

static void handle_ipv4(const uint8_t* frame, uint16_t len) {
    if (len < 34) return;
    const uint8_t* ip = frame + 14;
    uint8_t ihl = (uint8_t)((ip[0] & 0x0F) * 4);
    if (ihl < 20 || len < 14 + ihl) return;
    uint16_t total = bswap16(*(const uint16_t*)(ip + 2));
    if (14 + total > len) { status.dropped++; return; }
    if (checksum(ip, ihl) != 0) { status.checksum_errors++; return; }
    uint32_t src = bswap32(*(const uint32_t*)(ip + 12));
    uint32_t dst = bswap32(*(const uint32_t*)(ip + 16));
    if (dst != LOCAL_IP) return;
    arp_cache_put(src, frame + 6);
    if (ip[9] == IP_PROTO_ICMP) {
        const uint8_t* icmp = ip + ihl;
        uint16_t icmp_len = total - ihl;
        if (icmp_len < 8) { status.dropped++; return; }
        if (checksum(icmp, icmp_len) != 0) { status.checksum_errors++; return; }
        if (icmp[0] == 8) {
            send_icmp_echo_reply(frame, len);
        } else if (icmp[0] == 0) {
            uint16_t seq = bswap16(*(const uint16_t*)(icmp + 6));
            if (src == pending_ping_ip && seq == pending_ping_seq) status.last_ping_ok = 1;
        }
    } else if (ip[9] == IP_PROTO_UDP) {
        const uint8_t* udp = ip + ihl;
        uint16_t udp_len = total - ihl;
        if (udp_len < 8) { status.dropped++; return; }
        last_udp.src_ip = src;
        last_udp.src_port = bswap16(*(const uint16_t*)udp);
        last_udp.dst_port = bswap16(*(const uint16_t*)(udp + 2));
        last_udp.len = udp_len - 8;
        if (last_udp.len > sizeof(last_udp.data) - 1) last_udp.len = sizeof(last_udp.data) - 1;
        memcpy(last_udp.data, udp + 8, last_udp.len);
        last_udp.data[last_udp.len] = '\0';
        last_udp.valid = 1;
    }
}

int net_poll(void) {
    uint8_t frame[1600];
    uint16_t len = 0;
    int handled = 0;
    for (int i = 0; i < 8; i++) {
        int r = net_receive_packet(frame, &len);
        if (r <= 0 || len < 14) break;
        uint16_t type = bswap16(*(uint16_t*)(frame + 12));
        if (type == ETH_TYPE_ARP) handle_arp(frame, len);
        else if (type == ETH_TYPE_IPV4) handle_ipv4(frame, len);
        else status.dropped++;
        handled++;
    }
    return handled;
}

int net_format_arp_cache(char* buffer, uint32_t* size, uint32_t max_size) {
    if (!buffer || !size || max_size == 0) return -1;
    uint32_t pos = 0;
    append_str(buffer, &pos, max_size, "IP MAC\n");
    for (int i = 0; i < 8; i++) {
        if (!arp_cache[i].valid) continue;
        append_ip(buffer, &pos, max_size, arp_cache[i].ip);
        append_str(buffer, &pos, max_size, " ");
        append_mac(buffer, &pos, max_size, arp_cache[i].mac);
        append_str(buffer, &pos, max_size, " age=");
        append_u32(buffer, &pos, max_size, (uint32_t)(timer_get_ticks() - arp_cache[i].updated));
        append_str(buffer, &pos, max_size, "\n");
    }
    buffer[pos < max_size ? pos : max_size - 1] = '\0';
    *size = pos;
    return 0;
}

int net_format_device_stats(char* buffer, uint32_t* size, uint32_t max_size) {
    if (!buffer || !size || max_size == 0) return -1;
    uint32_t pos = 0;
    append_str(buffer, &pos, max_size, "iface driver rx tx irq rxerr drop csum mac\n");
    append_str(buffer, &pos, max_size, "eth0 ");
    append_str(buffer, &pos, max_size, net_driver_name(status.driver));
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.packets_rx);
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.packets_tx);
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.irq_count);
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.rx_errors);
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.dropped);
    append_str(buffer, &pos, max_size, " ");
    append_u32(buffer, &pos, max_size, status.checksum_errors);
    append_str(buffer, &pos, max_size, " ");
    append_mac(buffer, &pos, max_size, status.mac);
    append_str(buffer, &pos, max_size, "\n");
    buffer[pos < max_size ? pos : max_size - 1] = '\0';
    *size = pos;
    return 0;
}

int net_send_udp(uint32_t ip, uint16_t port, const char* data, uint16_t len) {
    if (!data || len > 256 || status.driver == NET_DRIVER_NONE) return -1;
    net_poll();
    arp_entry_t* e = arp_lookup(ip);
    if (!e) {
        if (send_arp_request(ip) != 0) return -1;
        for (uint32_t spin = 0; spin < PING_WAIT_SPINS; spin++) {
            net_poll();
            e = arp_lookup(ip);
            if (e) break;
        }
    }
    if (!e) return 0;
    uint16_t udp_len = (uint16_t)(8 + len);
    uint16_t ip_len = (uint16_t)(20 + udp_len);
    uint8_t p[14 + 20 + 8 + 256];
    memset(p, 0, sizeof(p));
    memcpy(p, e->mac, 6);
    memcpy(p + 6, status.mac, 6);
    *(uint16_t*)(p + 12) = bswap16(ETH_TYPE_IPV4);
    uint8_t* iphdr = p + 14;
    iphdr[0] = 0x45;
    *(uint16_t*)(iphdr + 2) = bswap16(ip_len);
    iphdr[8] = 64;
    iphdr[9] = IP_PROTO_UDP;
    *(uint32_t*)(iphdr + 12) = bswap32(LOCAL_IP);
    *(uint32_t*)(iphdr + 16) = bswap32(ip);
    *(uint16_t*)(iphdr + 10) = checksum(iphdr, 20);
    uint8_t* udp = iphdr + 20;
    *(uint16_t*)(udp + 0) = bswap16(49152);
    *(uint16_t*)(udp + 2) = bswap16(port);
    *(uint16_t*)(udp + 4) = bswap16(udp_len);
    memcpy(udp + 8, data, len);
    return net_send_packet(p, (uint16_t)(14 + ip_len));
}

int net_format_udp(char* buffer, uint32_t* size, uint32_t max_size) {
    if (!buffer || !size || max_size == 0) return -1;
    uint32_t pos = 0;
    if (!last_udp.valid) {
        append_str(buffer, &pos, max_size, "no udp packets\n");
    } else {
        append_str(buffer, &pos, max_size, "src=");
        append_ip(buffer, &pos, max_size, last_udp.src_ip);
        append_str(buffer, &pos, max_size, ":");
        append_u32(buffer, &pos, max_size, last_udp.src_port);
        append_str(buffer, &pos, max_size, " dst=");
        append_u32(buffer, &pos, max_size, last_udp.dst_port);
        append_str(buffer, &pos, max_size, " data=");
        append_str(buffer, &pos, max_size, last_udp.data);
        append_str(buffer, &pos, max_size, "\n");
    }
    buffer[pos < max_size ? pos : max_size - 1] = '\0';
    *size = pos;
    return 0;
}

uint32_t net_parse_ipv4(const char* s) {
    uint32_t parts[4] = {0,0,0,0};
    for (int i = 0; i < 4; i++) {
        if (!*s) return 0;
        while (*s >= '0' && *s <= '9') {
            parts[i] = parts[i] * 10 + (uint32_t)(*s - '0');
            if (parts[i] > 255) return 0;
            s++;
        }
        if (i < 3) { if (*s != '.') return 0; s++; }
    }
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}
