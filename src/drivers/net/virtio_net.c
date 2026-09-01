#include <core/asmh.h>
#include <core/errno.h>
#include <core/idt.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <core/printf.h>
#include <core/std.h>
#include <drivers/apic.h>
#include <drivers/net/virtio_net.h>
#include <drivers/pci.h>
#include <lib/string.h>
#include <lwip/lwip/netif.h>
#include <lwip/lwip/etharp.h>
#include <lwip/netif/ethernet.h>

#define VIRTIO_NET_RX_QUEUE 0
#define VIRTIO_NET_TX_QUEUE 1

#define VIRTIO_NET_NUM_RX_BUFS 32
#define VIRTIO_NET_BUF_SIZE    2048

static virtio_dev_t net_dev;
static virtqueue_t rx_vq;
static virtqueue_t tx_vq;

bool virtio_net_initialized = false;
static u8 net_mac[6] = {0};
static virtio_net_rx_callback_t rx_callback = NULL;

/* DMA buffers for RX and TX */
static u64 rx_buf_phys = 0;
static u8* rx_buf_virt = NULL;
static u16 rx_desc_map[VIRTIO_NET_NUM_RX_BUFS];

static u64 tx_buf_phys = 0;
static u8* tx_buf_virt = NULL;

extern void virtio_net_hdlr();

void c_virtio_net_hdlr(void) {
    if (!virtio_net_initialized) {
        lapic_eoi();
        return;
    }

    u8 isr = virtio_read_isr(&net_dev);
    (void)isr;

    /* Process received packets */
    while (virtqueue_has_used(&rx_vq)) {
        u32 len = 0;
        int desc_id = virtqueue_poll_used(&rx_vq, &len, 1);
        if (desc_id < 0) break;

        /* Find buffer index */
        int buf_idx = -1;
        for (int i = 0; i < VIRTIO_NET_NUM_RX_BUFS; i++) {
            if (rx_desc_map[i] == (u16)desc_id) {
                buf_idx = i;
                break;
            }
        }

        if (buf_idx >= 0 && len > sizeof(virtio_net_hdr_t)) {
            u8* pkt = rx_buf_virt + (buf_idx * VIRTIO_NET_BUF_SIZE) + sizeof(virtio_net_hdr_t);
            u16 pkt_len = (u16)(len - sizeof(virtio_net_hdr_t));

            if (rx_callback) {
                rx_callback(pkt, pkt_len);
            }
        }

        /* Re-queue descriptor */
        rx_vq.desc[desc_id].flags = VRING_DESC_F_WRITE;
        rx_vq.desc[desc_id].len = VIRTIO_NET_BUF_SIZE;
        virtqueue_submit_chain(&rx_vq, (u16)desc_id);
    }
    virtqueue_kick(&rx_vq);

    lapic_eoi();
}

static void virtio_net_rx_populate(void) {
    for (int i = 0; i < VIRTIO_NET_NUM_RX_BUFS && i < rx_vq.size; i++) {
        s32 desc = virtqueue_alloc_desc(&rx_vq);
        if (desc < 0) break;

        rx_desc_map[i] = (u16)desc;
        rx_vq.desc[desc].addr = rx_buf_phys + (i * VIRTIO_NET_BUF_SIZE);
        rx_vq.desc[desc].len = VIRTIO_NET_BUF_SIZE;
        rx_vq.desc[desc].flags = VRING_DESC_F_WRITE;
        rx_vq.desc[desc].next = 0;

        virtqueue_submit_chain(&rx_vq, (u16)desc);
    }
    virtqueue_kick(&rx_vq);
}

int virtio_net_init() {
    if (virtio_net_initialized) return 0;

    if (virtio_find_pci_device(VIRTIO_DEV_NET, &net_dev, 1) < 0) {
        return -ENOEXIST;
    }

    serial_printf("virtio-net: Found device at %02x:%02x.%d (iobase=0x%x, irq=%d)\n",
                  net_dev.bus, net_dev.slot, net_dev.fn, net_dev.iobase, net_dev.irq);

    /* Reset device */
    virtio_reset(&net_dev);

    /* Acknowledge & Driver */
    virtio_set_status(&net_dev, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(&net_dev, VIRTIO_STATUS_DRIVER);

    /* Negotiate features */
    u32 features = virtio_get_features(&net_dev);
    u32 guest_features = 0;
    if (features & VIRTIO_NET_F_MAC) {
        guest_features |= VIRTIO_NET_F_MAC;
    }
    if (features & VIRTIO_NET_F_STATUS) {
        guest_features |= VIRTIO_NET_F_STATUS;
    }
    virtio_set_features(&net_dev, guest_features);

    /* Read MAC address */
    for (int i = 0; i < 6; i++) {
        net_mac[i] = virtio_read_config8(&net_dev, (u8)i);
    }
    serial_printf("virtio-net: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                  net_mac[0], net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5]);

    /* Initialize RX and TX VirtQueues */
    if (virtqueue_init(&net_dev, VIRTIO_NET_RX_QUEUE, &rx_vq) < 0) {
        printf("virtio-net: Failed to initialize RX queue\n");
        virtio_set_status(&net_dev, VIRTIO_STATUS_FAILED);
        return -EDISK;
    }

    if (virtqueue_init(&net_dev, VIRTIO_NET_TX_QUEUE, &tx_vq) < 0) {
        printf("virtio-net: Failed to initialize TX queue\n");
        virtio_set_status(&net_dev, VIRTIO_STATUS_FAILED);
        return -EDISK;
    }

    /* Allocate RX and TX buffer pages */
    usize rx_pages = (VIRTIO_NET_NUM_RX_BUFS * VIRTIO_NET_BUF_SIZE + 4095) / 4096;
    rx_buf_phys = (u64)pmm_falloc(rx_pages);
    if (!rx_buf_phys) {
        printf("virtio-net: Failed to allocate RX buffers\n");
        virtio_set_status(&net_dev, VIRTIO_STATUS_FAILED);
        return -ENOMEM;
    }
    rx_buf_virt = (u8*)(HHDM_START + rx_buf_phys);
    memset(rx_buf_virt, 0, rx_pages * 4096);

    tx_buf_phys = (u64)pmm_falloc(1);
    if (!tx_buf_phys) {
        printf("virtio-net: Failed to allocate TX buffer\n");
        virtio_set_status(&net_dev, VIRTIO_STATUS_FAILED);
        return -ENOMEM;
    }
    tx_buf_virt = (u8*)(HHDM_START + tx_buf_phys);
    memset(tx_buf_virt, 0, 4096);

    /* Populate RX queue with receive buffers */
    virtio_net_rx_populate();

    /* Register IRQ handler */
    if (net_dev.irq > 0 && net_dev.irq < 24) {
        u8 vector = 0x20 + net_dev.irq;
        idt_regintr(NULL, vector, virtio_net_hdlr, 0x8E, 1);
        ioapic_set_irq(net_dev.irq, vector, get_lapic_id(), 0);
        ioapic_unmask_irq(net_dev.irq);
    }

    /* Driver OK */
    virtio_add_status(&net_dev, VIRTIO_STATUS_DRIVER_OK);

    virtio_net_initialized = true;
    return 0;
}

int virtio_net_send(const void* data, u16 len) {
    if (!virtio_net_initialized || !data || len == 0 || len > 1518) {
        return -EINVAL;
    }

    /* Format packet in TX buffer: virtio_net_hdr followed by raw Ethernet frame */
    virtio_net_hdr_t* hdr = (virtio_net_hdr_t*)tx_buf_virt;
    memset(hdr, 0, sizeof(virtio_net_hdr_t));

    u8* payload = tx_buf_virt + sizeof(virtio_net_hdr_t);
    memcpy(payload, data, len);

    s32 desc = virtqueue_alloc_desc(&tx_vq);
    if (desc < 0) {
        return -EFULL;
    }

    tx_vq.desc[desc].addr = tx_buf_phys;
    tx_vq.desc[desc].len = sizeof(virtio_net_hdr_t) + len;
    tx_vq.desc[desc].flags = 0; /* Device reads packet data */
    tx_vq.desc[desc].next = 0;

    virtqueue_submit_chain(&tx_vq, (u16)desc);
    virtqueue_kick(&tx_vq);

    /* Poll for transmission completion */
    int res = virtqueue_poll_used(&tx_vq, NULL, 1000000);
    virtqueue_free_desc(&tx_vq, (u16)desc);

    if (res < 0) {
        return -ETIME;
    }

    return 0;
}

void virtio_net_get_mac(u8* mac_out) {
    if (!mac_out) return;
    memcpy(mac_out, net_mac, 6);
}

bool virtio_net_link_up() {
    return virtio_net_initialized;
}

void virtio_net_set_rx_callback(virtio_net_rx_callback_t cb) {
    rx_callback = cb;
}

struct netif _virtio_netif = {0};

static err_t _virtio_netif_linkout(struct netif* netif, struct pbuf* p) {
    (void)netif;
    u8 frame[1518];
    u16 len = 0;

    for (struct pbuf* q = p; q != NULL; q = q->next) {
        if (len + q->len > sizeof(frame))
            return ERR_BUF;
        memcpy(frame + len, q->payload, q->len);
        len += q->len;
    }

    if (virtio_net_send(frame, len) < 0) {
        return ERR_IF;
    }

    return ERR_OK;
}

static void _virtio_netif_rxcb(const void* packet, u16 len) {
    struct pbuf* pb = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!pb) return;

    if (pbuf_take(pb, packet, len) != ERR_OK) {
        pbuf_free(pb);
        return;
    }

    if (_virtio_netif.input(pb, &_virtio_netif) != ERR_OK) {
        pbuf_free(pb);
        return;
    }
}

err_t virtio_net_netifinit(struct netif* nf) {
    serial_printf("Initializing VirtIO NetIF\n");
    nf->name[0] = 'v';
    nf->name[1] = 'n';

    nf->hwaddr_len = 6;
    memcpy(_virtio_netif.hwaddr, net_mac, 6);

    nf->mtu = 1500;
    nf->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_LINK_UP;
    nf->output = etharp_output;
    nf->linkoutput = _virtio_netif_linkout;
    nf->input = ethernet_input;
    virtio_net_set_rx_callback(_virtio_netif_rxcb);
    return ERR_OK;
}
