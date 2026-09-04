#include <core/errno.h>
#include <core/std.h>
#include <core/asmh.h>
#include <core/kprint.h>
#include <drivers/pci.h>
#include <drivers/virtio/virtio.h>
#include <core/mem/vmm.h>

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
    } else if (devid == VIRTIO_DEV_INPUT && hdr.devid == VIRTIO_DEV_MODERN_INPUT) {
        return 1;
    }

    return 0;
}

int virtio_pci_initfn(virtio_dev_t* dev, u32 bus, u32 slot, u32 fn, u32 inten) {
    pci_chdr_t hdr;
    pci_get_chdr_fn(bus, slot, fn, &hdr);

    dev->bus = (u8)bus;
    dev->slot = (u8)slot;
    dev->devid = hdr.devid;
    dev->irq = pci_cfg_inb(bus, slot, fn, 0x3C);
    dev->modern = 0;
    dev->common_cfg = NULL;
    dev->notify_cfg = NULL;
    dev->isr_cfg = NULL;
    dev->device_cfg = NULL;

    u8 cap = pci_cfg_inb(bus, slot, fn, 0x34);
    while (cap >= 0x40) {
        u8 cap_id = pci_cfg_inb(bus, slot, fn, cap);
        u8 cap_len = pci_cfg_inb(bus, slot, fn, cap + 2);
        u8 next = pci_cfg_inb(bus, slot, fn, cap + 1);
        if (cap_id == 0x09 && cap_len >= 16) {
            u8 cfg_type = pci_cfg_inb(bus, slot, fn, cap + 3);
            u8 bar = pci_cfg_inb(bus, slot, fn, cap + 4);
            u32 offset = pci_cfg_inl(bus, slot, fn, cap + 8);
            u32 length = pci_cfg_inl(bus, slot, fn, cap + 12);
            if (bar < 6 && length != 0) {
                u64 base = pci_read_bar64(bus, slot, fn, bar);
                volatile u8* region = (volatile u8*)(HHDM_START + base + offset);
                if (cfg_type == 1) dev->common_cfg = region;
                else if (cfg_type == 2) {
                    dev->notify_cfg = region;
                    if (cap_len >= 20) dev->notify_off_multiplier = pci_cfg_inl(bus, slot, fn, cap + 16);
                } else if (cfg_type == 3) dev->isr_cfg = region;
                else if (cfg_type == 4) dev->device_cfg = region;
            }
        }
        cap = next;
    }

    /* Enable Bus Master, Memory Space, and I/O Space in PCI Command */
    u16 pcicmd = pci_cfg_inw(bus, slot, fn, 0x04);
    pcicmd |= (1 << 0) | (1 << 1) | (1 << 2);

    if (inten) {
        pcicmd &= ~(1 << 10);
    } else {
        pcicmd |= (1 << 10);
    }

    pci_cfg_outw(bus, slot, fn, 0x04, pcicmd);

    if (dev->common_cfg && dev->notify_cfg && dev->isr_cfg && dev->device_cfg) {
        dev->modern = 1;
        return 0;
    }

    u32 bar0 = pci_read_bar(bus, slot, fn, 0);
    if (!(bar0 & 0x01)) return -EINVAL;
    dev->iobase = (u64)(bar0 & ~0x3);
    return 0;
}

int virtio_find_pci_device(u16 devid, virtio_dev_t* dev, u8 inten) {
    return virtio_find_pci_device_nth(devid, dev, inten, 0);
}

int virtio_find_pci_device_nth(u16 devid, virtio_dev_t* dev, u8 inten, u8 nth) {
    if (!dev) return -EINVAL;

    u8 found = 0;
    for (u32 bus = 0; bus < 256; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            u8 maxfn = virtio_pci_maxfn(bus, slot);
            if (!maxfn) continue;
            for (u32 fn = 0; fn < maxfn; fn++) {
                if (virtio_pci_isfunc(bus, slot, fn, devid)) {
                    if (found++ != nth) continue;
                    return virtio_pci_initfn(dev, bus, slot, fn, inten);
                }
            }
        }
    }

    return -1;
}

void virtio_reset(virtio_dev_t* dev) {
    if (!dev) return;
    if (dev->modern) dev->common_cfg[0x14] = VIRTIO_STATUS_RESET;
    else outb(dev->iobase + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_RESET);
}

u8 virtio_get_status(virtio_dev_t* dev) {
    if (!dev) return 0;
    return dev->modern ? dev->common_cfg[0x14] : inb(dev->iobase + VIRTIO_REG_DEVICE_STATUS);
}

void virtio_set_status(virtio_dev_t* dev, u8 status) {
    if (!dev) return;
    if (dev->modern) dev->common_cfg[0x14] = status;
    else outb(dev->iobase + VIRTIO_REG_DEVICE_STATUS, status);
}

void virtio_add_status(virtio_dev_t* dev, u8 status) {
    if (!dev) return;
    u8 curr = virtio_get_status(dev);
    virtio_set_status(dev, curr | status);
}

u32 virtio_get_features(virtio_dev_t* dev) {
    if (!dev) return 0;
    if (dev->modern) {
        *(volatile u32*)(dev->common_cfg + 0x00) = 0;
        return *(volatile u32*)(dev->common_cfg + 0x04);
    }
    return inl(dev->iobase + VIRTIO_REG_DEVICE_FEATURES);
}

void virtio_set_features(virtio_dev_t* dev, u32 features) {
    if (!dev) return;
    if (dev->modern) {
        u64 device_features = virtio_get_features64(dev);
        *(volatile u32*)(dev->common_cfg + 0x08) = 0;
        *(volatile u32*)(dev->common_cfg + 0x0c) = features;
        *(volatile u32*)(dev->common_cfg + 0x08) = 1;
        *(volatile u32*)(dev->common_cfg + 0x0c) = (u32)(device_features >> 32) & 1;
        virtio_add_status(dev, VIRTIO_STATUS_FEATURES_OK);
        return;
    }
    outl(dev->iobase + VIRTIO_REG_GUEST_FEATURES, features);
}

u64 virtio_get_features64(virtio_dev_t* dev) {
    if (!dev) return 0;
    if (!dev->modern) return virtio_get_features(dev);
    volatile u8* cfg = dev->common_cfg;
    *(volatile u32*)(cfg + 0x00) = 0;
    u64 lo = *(volatile u32*)(cfg + 0x04);
    *(volatile u32*)(cfg + 0x00) = 1;
    u64 hi = *(volatile u32*)(cfg + 0x04);
    return lo | (hi << 32);
}

void virtio_set_features64(virtio_dev_t* dev, u64 features) {
    if (!dev) return;
    if (!dev->modern) {
        virtio_set_features(dev, (u32)features);
        return;
    }
    volatile u8* cfg = dev->common_cfg;
    *(volatile u32*)(cfg + 0x08) = 0;
    *(volatile u32*)(cfg + 0x0c) = (u32)features;
    *(volatile u32*)(cfg + 0x08) = 1;
    *(volatile u32*)(cfg + 0x0c) = (u32)(features >> 32);
}

u8 virtio_read_config8(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return dev->modern ? dev->device_cfg[offset] : inb(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u16 virtio_read_config16(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return dev->modern ? *(volatile u16*)(dev->device_cfg + offset) : inw(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u32 virtio_read_config32(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    return dev->modern ? *(volatile u32*)(dev->device_cfg + offset) : inl(dev->iobase + VIRTIO_REG_CONFIG_LEGACY + offset);
}

u64 virtio_read_config64(virtio_dev_t* dev, u8 offset) {
    if (!dev) return 0;
    u32 lo = virtio_read_config32(dev, offset);
    u32 hi = virtio_read_config32(dev, offset + 4);
    return ((u64)hi << 32) | lo;
}

u8 virtio_read_isr(virtio_dev_t* dev) {
    if (!dev) return 0;
    return dev->modern ? *dev->isr_cfg : inb(dev->iobase + VIRTIO_REG_ISR_STATUS);
}
