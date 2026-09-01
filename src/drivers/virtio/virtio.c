#include <core/std.h>
#include <core/asmh.h>
#include <core/printf.h>
#include <drivers/pci.h>
#include <drivers/virtio/virtio.h>

int virtio_find_pci_device(u16 devid, virtio_dev_t* dev) {
    if (!dev) return -1;

    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            pci_chdr_t hdr;
            pci_get_chdr_fn(bus, slot, 0, &hdr);
            if (hdr.vndid == 0xFFFF || hdr.vndid == 0) continue;

            u8 max_fn = (hdr.hdrt & 0x80) ? 8 : 1;
            for (u32 fn = 0; fn < max_fn; fn++) {
                if (fn > 0) {
                    pci_get_chdr_fn(bus, slot, fn, &hdr);
                    if (hdr.vndid == 0xFFFF || hdr.vndid == 0) continue;
                }

                if (hdr.vndid != VIRTIO_VENDOR_ID) continue;

                bool match = false;
                if (hdr.devid == devid) {
                    match = true;
                } else if (devid == VIRTIO_DEV_NET && hdr.devid == VIRTIO_DEV_MODERN_NET) {
                    match = true;
                } else if (devid == VIRTIO_DEV_BLOCK && hdr.devid == VIRTIO_DEV_MODERN_BLOCK) {
                    match = true;
                } else if (devid == VIRTIO_DEV_RNG && hdr.devid == VIRTIO_DEV_MODERN_RNG) {
                    match = true;
                }

                if (match) {
                    u32 bar0 = pci_read_bar(bus, slot, fn, 0);
                    if (!(bar0 & 0x01)) {
                        /* BAR0 must be I/O space for legacy VirtIO */
                        continue;
                    }

                    dev->bus = (u8)bus;
                    dev->slot = (u8)slot;
                    dev->fn = (u8)fn;
                    dev->devid = hdr.devid;
                    dev->iobase = (u16)(bar0 & ~0x3);
                    dev->irq = pci_cfg_inb(bus, slot, fn, 0x3C);

                    /* Enable Bus Master, Memory Space, and I/O Space in PCI Command */
                    u16 pci_cmd = pci_cfg_inw(bus, slot, fn, 0x04);
                    pci_cmd |= (1 << 0) | (1 << 1) | (1 << 2);
                    pci_cmd &= ~(1 << 10); /* Enable legacy interrupts */
                    pci_cfg_outw(bus, slot, fn, 0x04, pci_cmd);

                    return 0;
                }
            }
        }
    }

    return -1;
}

void virtio_reset(virtio_dev_t* dev) {
    if (!dev) return;
    outb(dev->iobase + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_RESET);
}

u8 virtio_get_status(virtio_dev_t* dev) {
    if (!dev) return 0;
    return inb(dev->iobase + VIRTIO_REG_DEVICE_STATUS);
}

void virtio_set_status(virtio_dev_t* dev, u8 status) {
    if (!dev) return;
    outb(dev->iobase + VIRTIO_REG_DEVICE_STATUS, status);
}

void virtio_add_status(virtio_dev_t* dev, u8 status) {
    if (!dev) return;
    u8 curr = inb(dev->iobase + VIRTIO_REG_DEVICE_STATUS);
    outb(dev->iobase + VIRTIO_REG_DEVICE_STATUS, curr | status);
}

u32 virtio_get_features(virtio_dev_t* dev) {
    if (!dev) return 0;
    return inl(dev->iobase + VIRTIO_REG_DEVICE_FEATURES);
}

void virtio_set_features(virtio_dev_t* dev, u32 features) {
    if (!dev) return;
    outl(dev->iobase + VIRTIO_REG_GUEST_FEATURES, features);
}

u8 virtio_read_config8(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return inb(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u16 virtio_read_config16(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return inw(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u32 virtio_read_config32(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return inl(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u64 virtio_read_config64(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    u32 lo = inl(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
    u32 hi = inl(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset + 4);
    return ((u64)hi << 32) | lo;
}

u8 virtio_read_isr(virtio_dev_t* dev) {
    if (!dev) return 0;
    return inb(dev->iobase + VIRTIO_REG_ISR_STATUS);
}
