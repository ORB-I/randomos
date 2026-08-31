#include <core/asmh.h>
#include <core/errno.h>
#include <core/idt.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <core/printf.h>
#include <core/std.h>
#include <drivers/apic.h>
#include <drivers/net/e1000.h>
#include <drivers/pci.h>
#include <lib/string.h>
#include <lwip/lwip/netif.h>
#include <lwip/lwip/etharp.h>
#include <lwip/netif/ethernet.h>

#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_ETHERNET 0x00
#define INTEL_VENDOR_ID 0x8086

/* Known Intel E1000 Device IDs ig */
#define E1000_DEV_82540EM 0x100E /* Standard QEMU e1000 */
#define E1000_DEV_82545EM_COPPER 0x100F
#define E1000_DEV_82543GC 0x1004
#define E1000_DEV_82544EI_COPPER 0x1008
#define E1000_DEV_82544EI_FIBER 0x1009
#define E1000_DEV_82544GC_COPPER 0x100C
#define E1000_DEV_82544GC_LOM 0x100D
#define E1000_DEV_82546EB_COPPER 0x1010
#define E1000_DEV_82546EB_FIBER 0x1012
#define E1000_DEV_82546EB_QUAD 0x101D
#define E1000_DEV_82547EI 0x1019
#define E1000_DEV_82547EI_MOBILE 0x101A
#define E1000_DEV_82546GB_COPPER 0x1079
#define E1000_DEV_82546GB_FIBER 0x107A
#define E1000_DEV_82546GB_SERDES 0x107B
#define E1000_DEV_82574L 0x10D3 /* QEMU e1000e */
#define E1000_DEV_82577LM 0x10EA
#define E1000_DEV_I217_LM 0x153A
#define E1000_DEV_I219_LM 0x156F

static u8 e1000_bus = 0;
static u8 e1000_slot = 0;
static u8 e1000_fn = 0;
static u16 e1000_device_id = 0;

static u64 e1000_mmio_phys = 0;
static u64 e1000_mmio_base = 0;
static u8 e1000_mac[6] = {0};
static u8 e1000_irq = 0;
static bool e1000_initialized = false;

static e1000_rx_desc_t *rx_descs = NULL;
static u64 rx_desc_phys = 0;
static u8 *rx_bufs = NULL;
static u64 rx_buf_phys = 0;
static u32 rx_cur = 0;

static e1000_tx_desc_t *tx_descs = NULL;
static u64 tx_desc_phys = 0;
static u8 *tx_bufs = NULL;
static u64 tx_buf_phys = 0;
static u32 tx_cur = 0;

static e1000_rx_callback_t rx_callback = NULL;

extern void e1000_hdlr();

static inline u32 e1000_read(u32 reg) {
    return *((volatile u32 *)(e1000_mmio_base + reg));
}

static inline void e1000_write(u32 reg, u32 val) {
    *((volatile u32 *)(e1000_mmio_base + reg)) = val;
}

static bool e1000_is_supported_device(u16 devid) {
    switch (devid) {
    case E1000_DEV_82540EM:
    case E1000_DEV_82545EM_COPPER:
    case E1000_DEV_82543GC:
    case E1000_DEV_82544EI_COPPER:
    case E1000_DEV_82544EI_FIBER:
    case E1000_DEV_82544GC_COPPER:
    case E1000_DEV_82544GC_LOM:
    case E1000_DEV_82546EB_COPPER:
    case E1000_DEV_82546EB_FIBER:
    case E1000_DEV_82546EB_QUAD:
    case E1000_DEV_82547EI:
    case E1000_DEV_82547EI_MOBILE:
    case E1000_DEV_82546GB_COPPER:
    case E1000_DEV_82546GB_FIBER:
    case E1000_DEV_82546GB_SERDES:
    case E1000_DEV_82574L:
    case E1000_DEV_82577LM:
    case E1000_DEV_I217_LM:
    case E1000_DEV_I219_LM:
        return true;
    default:
        return false;
    }
}

static int e1000_find_pci(void) {
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
        for (u8 fn = 0; fn < 8; fn++) {
            pci_chdr_t hdr;
            pci_get_chdr_fn(bus, slot, fn, &hdr);
            if (hdr.vndid == 0xFFFF || hdr.vndid == 0) continue;
            if (hdr.vndid == INTEL_VENDOR_ID) {
                if (e1000_is_supported_device(hdr.devid) ||
                    (hdr.cls == PCI_CLASS_NETWORK &&
                    hdr.subcls == PCI_SUBCLASS_ETHERNET)) {
                    e1000_bus = (u8)bus;
                    e1000_slot = slot;
                    e1000_fn = fn;
                    e1000_device_id = hdr.devid;
                    return 0;
                }
            }
        }
        }
    }
    return -ENOEXIST;
}

static u16 e1000_read_eeprom(u8 addr) {
    e1000_write(E1000_REG_EERD,
              E1000_EERD_START | ((u32)addr << E1000_EERD_ADDR_SHIFT));
    for (u32 i = 0; i < 100000; i++) {
        u32 val = e1000_read(E1000_REG_EERD);
        if (val & E1000_EERD_DONE) {
        return (u16)((val >> E1000_EERD_DATA_SHIFT) & 0xFFFF);
        }
    }
    return 0xFFFF;
}

static void e1000_read_mac(void) {
    u16 w0 = e1000_read_eeprom(0);
    u16 w1 = e1000_read_eeprom(1);
    u16 w2 = e1000_read_eeprom(2);

    if (w0 != 0xFFFF || w1 != 0xFFFF || w2 != 0xFFFF) {
        e1000_mac[0] = (u8)(w0 & 0xFF);
        e1000_mac[1] = (u8)((w0 >> 8) & 0xFF);
        e1000_mac[2] = (u8)(w1 & 0xFF);
        e1000_mac[3] = (u8)((w1 >> 8) & 0xFF);
        e1000_mac[4] = (u8)(w2 & 0xFF);
        e1000_mac[5] = (u8)((w2 >> 8) & 0xFF);
    } else {
        u32 ral = e1000_read(E1000_REG_RAL);
        u32 rah = e1000_read(E1000_REG_RAH);
        e1000_mac[0] = (u8)(ral & 0xFF);
        e1000_mac[1] = (u8)((ral >> 8) & 0xFF);
        e1000_mac[2] = (u8)((ral >> 16) & 0xFF);
        e1000_mac[3] = (u8)((ral >> 24) & 0xFF);
        e1000_mac[4] = (u8)(rah & 0xFF);
        e1000_mac[5] = (u8)((rah >> 8) & 0xFF);
    }

    /* Program MAC into RAL0 / RAH0 to guarantee hardware filtering */
    u32 ral_val = (u32)e1000_mac[0] | ((u32)e1000_mac[1] << 8) |
                    ((u32)e1000_mac[2] << 16) | ((u32)e1000_mac[3] << 24);
    u32 rah_val = (u32)e1000_mac[4] | ((u32)e1000_mac[5] << 8) | E1000_RAH_AV;
    e1000_write(E1000_REG_RAL, ral_val);
    e1000_write(E1000_REG_RAH, rah_val);
}

static void e1000_rx_init(void) {
    /* 1 page for 32 descriptors (512 bytes needed) */
    rx_desc_phys = (u64)pmm_falloc(1);
    rx_descs = (e1000_rx_desc_t *)(HHDM_START + rx_desc_phys);
    memset(rx_descs, 0, 4096);

    /* 16 pages for 32 x 2048-byte packet buffers (64KB) */
    rx_buf_phys = (u64)pmm_falloc(16);
    rx_bufs = (u8 *)(HHDM_START + rx_buf_phys);
    memset(rx_bufs, 0, 16 * 4096);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr = rx_buf_phys + (i * E1000_PKT_BUF_SIZE);
        rx_descs[i].status = 0;
        rx_descs[i].errors = 0;
        rx_descs[i].length = 0;
        rx_descs[i].checksum = 0;
        rx_descs[i].special = 0;
    }

    e1000_write(E1000_REG_RDBAL, (u32)(rx_desc_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_RDBAH, (u32)((rx_desc_phys >> 32) & 0xFFFFFFFF));
    e1000_write(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    rx_cur = 0;

    /* Enable receiver, broadcast accept, 2048-byte buffers, strip Ethernet CRC */
    u32 rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 |
                E1000_RCTL_SECRC | E1000_RCTL_RDMTS_HALF;
    e1000_write(E1000_REG_RCTL, rctl);
}

static void e1000_tx_init(void) {
    /* 1 page for 32 descriptors (512 bytes needed) */
    tx_desc_phys = (u64)pmm_falloc(1);
    tx_descs = (e1000_tx_desc_t *)(HHDM_START + tx_desc_phys);
    memset(tx_descs, 0, 4096);

    /* 16 pages for 32 x 2048-byte packet buffers (64KB) */
    tx_buf_phys = (u64)pmm_falloc(16);
    tx_bufs = (u8 *)(HHDM_START + tx_buf_phys);
    memset(tx_bufs, 0, 16 * 4096);

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr = tx_buf_phys + (i * E1000_PKT_BUF_SIZE);
        tx_descs[i].cmd = 0;
        tx_descs[i].status = E1000_TXD_STAT_DD;
        tx_descs[i].length = 0;
        tx_descs[i].cso = 0;
        tx_descs[i].css = 0;
        tx_descs[i].special = 0;
    }

    e1000_write(E1000_REG_TDBAL, (u32)(tx_desc_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_TDBAH, (u32)((tx_desc_phys >> 32) & 0xFFFFFFFF));
    e1000_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    tx_cur = 0;

    /* IPGT = 10, IPGR1 = 8, IPGR2 = 6 */
    e1000_write(E1000_REG_TIPG, 10 | (8 << 10) | (6 << 20));

    /* Enable transmitter, pad short packets, collision parameters */
    u32 tctl = E1000_TCTL_EN | E1000_TCTL_PSP | (15 << E1000_TCTL_CT_SHIFT) |
                (64 << E1000_TCTL_COLD_SHIFT) | E1000_TCTL_RTLC;
    e1000_write(E1000_REG_TCTL, tctl);
}

void c_e1000_hdlr(void) {
    if (!e1000_initialized) {
        lapic_eoi();
        return;
    }

    u32 icr = e1000_read(E1000_REG_ICR);

    if (icr & E1000_ICR_LSC) {
        u32 status = e1000_read(E1000_REG_STATUS);
        if (status & E1000_STATUS_LU) {
        const char *speed_str = "10 Mbps";
        if ((status & E1000_STATUS_SPEED_MASK) == E1000_STATUS_SPEED_1G) {
            speed_str = "1000 Mbps (Gigabit)";
        } else if ((status & E1000_STATUS_SPEED_MASK) ==
                    E1000_STATUS_SPEED_100M) {
            speed_str = "100 Mbps";
        }
        printf("e1000: Link status UP (%s, %s)\n", speed_str,
                (status & E1000_STATUS_FD) ? "Full-Duplex" : "Half-Duplex");
        } else {
        printf("e1000: Link status DOWN\n");
        }
    }

    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)) {
        while (rx_descs[rx_cur].status & E1000_RXD_STAT_DD) {
        u16 len = rx_descs[rx_cur].length;
        u8 *buf = rx_bufs + (rx_cur * E1000_PKT_BUF_SIZE);

        if (rx_callback && len > 0) {
            rx_callback(buf, len);
        }

        rx_descs[rx_cur].status = 0;
        u32 old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
        e1000_write(E1000_REG_RDT, old_cur);
        }
    }

    lapic_eoi();
}

int e1000_init(void) {
    if (e1000_find_pci() < 0) {
        printf("e1000: No Intel E1000 network adapter found on PCI\n");
        return -ENOEXIST;
    }

    printf("e1000: Found device %04x on PCI %02x:%02x.%x\n", e1000_device_id,
            e1000_bus, e1000_slot, e1000_fn);

    /* Read BAR0 (MMIO Base) */
    u32 bar0_low = pci_read_bar(e1000_bus, e1000_slot, e1000_fn, 0);
    u32 bar0_high = 0;
    if ((bar0_low & 0x6) == 0x4) { /* 64-bit BAR */
        bar0_high = pci_read_bar(e1000_bus, e1000_slot, e1000_fn, 1);
    }

    e1000_mmio_phys = ((u64)bar0_high << 32) | (bar0_low & ~0xFULL);
    if (!e1000_mmio_phys) {
        printf("e1000: Invalid BAR0 MMIO address\n");
        return -EINVAL;
    }

    e1000_mmio_base = HHDM_START + e1000_mmio_phys;

    /* Enable PCI Bus Master & Memory Space, clear INTx disable */
    u16 pci_cmd = pci_cfg_inw(e1000_bus, e1000_slot, e1000_fn, 0x04);
    pci_cmd |= (1 << 0) | (1 << 1) | (1 << 2); /* I/O, Mem, Bus Master */
    pci_cmd &= ~(1 << 10);                     /* Enable legacy interrupts */
    pci_cfg_outw(e1000_bus, e1000_slot, e1000_fn, 0x04, pci_cmd);

    /* Read PCI Interrupt Line */
    e1000_irq = pci_cfg_inb(e1000_bus, e1000_slot, e1000_fn, 0x3C);

    /* Disable interrupts and reset device */
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);
    (void)e1000_read(E1000_REG_ICR);

    u32 ctrl = e1000_read(E1000_REG_CTRL);
    e1000_write(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);

    /* Wait for reset to complete */
    for (volatile u32 i = 0; i < 100000; i++) {
        if (!(e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST)) break;
    }

    /* Disable interrupts again after reset */
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);
    (void)e1000_read(E1000_REG_ICR);

    /* Read MAC address */
    e1000_read_mac();
    printf("e1000: Hardware MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", e1000_mac[0],
            e1000_mac[1], e1000_mac[2], e1000_mac[3], e1000_mac[4], e1000_mac[5]);

    /* Clear Multicast Table Array (MTA) */
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_REG_MTA + (i * 4), 0);
    }

    /* Initialize RX and TX DMA rings */
    e1000_rx_init();
    e1000_tx_init();

    /* Establish link (Set Link Up & Auto-Speed Detection) */
    ctrl = e1000_read(E1000_REG_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD;
    ctrl &= ~(E1000_CTRL_PHY_RST | E1000_CTRL_LRST | E1000_CTRL_ILOS);
    e1000_write(E1000_REG_CTRL, ctrl);

    /* Setup Interrupt handling if IRQ line is configured */
    if (e1000_irq > 0 && e1000_irq < 24) {
        u8 vector = 0x20 + e1000_irq;
        idt_regintr(NULL, vector, e1000_hdlr, 0x8E, 1);
        ioapic_set_irq(e1000_irq, vector, get_lapic_id(), 0);
        ioapic_unmask_irq(e1000_irq);

        /* Enable desired interrupt events */
        e1000_write(E1000_REG_IMS, E1000_ICR_RXT0 | E1000_ICR_TXDW | E1000_ICR_LSC |
                                    E1000_ICR_RXDMT0);
        (void)e1000_read(E1000_REG_ICR);
    }

    e1000_initialized = true;

    u32 status = e1000_read(E1000_REG_STATUS);
    printf("e1000: Controller initialized (Link: %s)\n",
            (status & E1000_STATUS_LU) ? "UP" : "DOWN");

    return 0;
}

int e1000_send(const void *data, u16 len) {
    if (!e1000_initialized || !data || len == 0 || len > E1000_PKT_BUF_SIZE) {
        return -EINVAL;
    }

    u32 idx = tx_cur;

    /* Wait for previous transmission on this descriptor to finish */
    for (u32 retry = 0; retry < 100000; retry++) {
        if (tx_descs[idx].status & E1000_TXD_STAT_DD)
        break;
    }

    if (!(tx_descs[idx].status & E1000_TXD_STAT_DD)) {
        return -EHANG;
    }

    u8 *buf = tx_bufs + (idx * E1000_PKT_BUF_SIZE);
    memcpy(buf, data, len);

    tx_descs[idx].length = len;
    tx_descs[idx].status = 0;
    tx_descs[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_REG_TDT, tx_cur);

    return 0;
}

int e1000_receive(void *buf, u16 max_len) {
    if (!e1000_initialized || !buf || max_len == 0) {
        return -EINVAL;
    }

    u32 idx = rx_cur;
    if (!(rx_descs[idx].status & E1000_RXD_STAT_DD)) {
        return -EHANG;
    }

    u16 len = rx_descs[idx].length;
    if (len > max_len) {
        len = max_len;
    }

    u8 *src = rx_bufs + (idx * E1000_PKT_BUF_SIZE);
    memcpy(buf, src, len);

    rx_descs[idx].status = 0;
    u32 old_cur = idx;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_REG_RDT, old_cur);

    return (int)len;
}

void e1000_get_mac(u8 *mac_out) {
    if (!mac_out)
        return;
    memcpy(mac_out, e1000_mac, 6);
}

bool e1000_link_up(void) {
    if (!e1000_initialized)
        return false;
    return (e1000_read(E1000_REG_STATUS) & E1000_STATUS_LU) != 0;
}

void e1000_set_rx_callback(e1000_rx_callback_t cb) { rx_callback = cb; }

struct netif _e1000_netif = {0};

err_t _e1000_netif_linkout(struct netif *netif, struct pbuf *p) {
    (void)netif;
    u8 frame[1518];
    u16 len = 0;

    for (struct pbuf *q = p; q != NULL; q = q->next) {
        if (len + q->len > sizeof(frame))
            return ERR_BUF;
        memcpy(frame + len, q->payload, q->len);
        len += q->len;
    }

    if (e1000_send(frame, len) < 0) {
        return ERR_IF;
    }

    return ERR_OK;
}

void _e1000_netif_rxcb(const void* packet, u16 len) {
    struct pbuf* pb = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!pb) return;

    if (pbuf_take(pb, packet, len) != ERR_OK) {
        pbuf_free(pb);
        return;
    }

    if (_e1000_netif.input(pb, &_e1000_netif) != ERR_OK) {
        pbuf_free(pb);
        return;
    }
}

err_t e1000_netifinit(struct netif* nf) {
    serial_printf("Initializing E1000 NetIF\n");
    nf->name[0] = 'e';
    nf->name[1] = 'n';

    nf->hwaddr_len = 6;
    memcpy(_e1000_netif.hwaddr, e1000_mac, 6);
    
    nf->mtu = 1500;
    nf->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    nf->output = etharp_output;
    nf->linkoutput = _e1000_netif_linkout;
    nf->input = ethernet_input;
    e1000_set_rx_callback(_e1000_netif_rxcb);
    return ERR_OK;
}