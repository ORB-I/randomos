#pragma once
#include <core/std.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <lwip/lwip/netif.h>

/* VirtIO Net Feature bits */
#define VIRTIO_NET_F_CSUM       (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM (1 << 1)
#define VIRTIO_NET_F_MAC        (1 << 5)
#define VIRTIO_NET_F_STATUS     (1 << 16)

/* Legacy VirtIO Network Header (10 bytes) */
typedef struct {
    u8 flags;
    u8 gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

typedef void (*virtio_net_rx_callback_t)(const void* data, u16 len);

int virtio_net_init();
int virtio_net_send(const void* data, u16 len);
int virtio_net_receive(void* buf, u16 max_len);
void virtio_net_get_mac(u8* mac_out);
bool virtio_net_link_up();
void virtio_net_set_rx_callback(virtio_net_rx_callback_t cb);
err_t virtio_net_netifinit(struct netif* netif);

extern struct netif _virtio_netif;
extern bool virtio_net_initialized;
