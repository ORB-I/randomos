#include <core/errno.h>
#include <core/std.h>
#include <core/asmh.h>
#include <core/kprint.h>
#include <drivers/pci.h>
#include <drivers/virtio/virtio.h>

int virtio_pci_maxfn(u32 bus, u32 slot) {
    pci_chdr_t hdr;
    pci_get_chdr_fn(bus, slot, 0, &hdr);
    if (hdr.vndid == 0xFFFF || hdr.vndid == 0) return 0;
    return (hdr.hdrt & 0x80) ? 8 : 1;
}

int virtio_pci_isfunc(u32 bus, u32 slot, u32 fn, u16 devid) {
    pci_chdr_t hdr;
    pci_get_chdr_fn(bus, slot, fn, &hdr);
    if (hdr.vndid == 0xFFFF || hdr.vndid == 0 || hdr.vndid != VIRTIO_VENDOR_ID) return 0;
    if (hdr.devid == devid) {
        return 1;
    } else if (devid == VIRTIO_DEV_NET && hdr.devid == VIRTIO_DEV_MODERN_NET) {
        return 1;
    } else if (devid == VIRTIO_DEV_BLOCK && hdr.devid == VIRTIO_DEV_MODERN_BLOCK) {
        return 1;
    } else if (devid == VIRTIO_DEV_RNG && hdr.devid == VIRTIO_DEV_MODERN_RNG) {
        return 1;
    }

    return 0;
}

int virtio_pci_initfn(virtio_dev_t* dev, u32 bus, u32 slot, u32 fn, u32 inten) {
    pci_chdr_t hdr;
    pci_get_chdr_fn(bus, slot, fn, &hdr);

    u32 bar0 = pci_read_bar(bus, slot, fn, 0);
    if (!(bar0 & 0x01)) {
        return -EINVAL;
    }

    dev->bus = (u8)bus;
    dev->slot = (u8)slot;
    dev->devid = hdr.devid;
    dev->iobase = (u64)(bar0 & ~0x3);
    dev->irq = pci_cfg_inb(bus, slot, fn, 0x3C);

    /* Enable Bus Master, Memory Space, and I/O Space in PCI Command */
    u16 pcicmd = pci_cfg_inw(bus, slot, fn, 0x04);
    pcicmd |= (1 << 0) | (1 << 1) | (1 << 2);

    if (inten) {
        pcicmd &= ~(1 << 10);
    } else {
        pcicmd |= (1 << 10);
    }

    pci_cfg_outw(bus, slot, fn, 0x04, pcicmd);
    return 0;
}

int virtio_find_pci_device(u16 devid, virtio_dev_t* dev, u8 inten) {
    if (!dev) return -EINVAL;

    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            u8 maxfn = virtio_pci_maxfn(bus, slot);
            if (!maxfn) continue;
            for (u32 fn = 0; fn < maxfn; fn++) {
                if (virtio_pci_isfunc(bus, slot, fn, devid)) {
                    return virtio_pci_initfn(dev, bus, slot, fn, inten);
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
