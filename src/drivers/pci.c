#include <core/std.h>
#include <core/asmh.h>

#include <drivers/pci.h>

#define PCI_ARGS u8 bus, u8 slot, u8 fn, u8 off

u32 pci_addr(u8 bus, u8 slot, u8 fn, u8 off) {
    return (u32)(
        ((u32)bus << 16)  |
        ((u32)slot << 11) |
        ((u32)fn << 8)    |
        (off & 0xFC)  |
        ((u32)0x80000000)
    );
}

u8 pci_cfg_inb(u8 bus, u8 slot, u8 fn, u8 off) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    return (u8)((inl(0xCFC) >> ((off & 3) * 8)) & 0xFF);
}

u16 pci_cfg_inw(u8 bus, u8 slot, u8 fn, u8 off) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    return (u16)((inl(0xCFC) >> ((off & 2) * 8)) & 0xFFFF);
}

u32 pci_cfg_inl(u8 bus, u8 slot, u8 fn, u8 off) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    return inl(0xCFC);
}

void pci_cfg_outb(u8 bus, u8 slot, u8 fn, u8 off, u8 val) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    outl(0xCFC, (inl(0xCFC) & ~(0xFF << ((off & 3) * 8))) | ((u32)val << ((off & 3) * 8)));
}

void pci_cfg_outw(u8 bus, u8 slot, u8 fn, u8 off, u16 val) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    outl(0xCFC, (inl(0xCFC) & ~(0xFFFF << ((off & 2) * 8))) | ((u32)val << ((off & 2) * 8)));
}

void pci_cfg_outl(u8 bus, u8 slot, u8 fn, u8 off, u32 val) {
    outl(0xCF8, pci_addr(bus, slot, fn, off));
    outl(0xCFC, val);
}

void pci_get_chdr(u32 bus, u32 slot, pci_chdr_t* hdr) {
    u32 r0 = pci_cfg_inl(bus, slot, 0, 0);
    u32 r1 = pci_cfg_inl(bus, slot, 1, 0);
    u32 r2 = pci_cfg_inl(bus, slot, 2, 0);
    u32 r3 = pci_cfg_inl(bus, slot, 3, 0);

    hdr->devid = (u16)(r0 & 0xFFFF0000);
    hdr->vndid = (u16)(r0 & 0x0000FFFF);
    hdr->stat = (u16)(r1 & 0xFFFF0000);
    hdr->cmd = (u16)(r1 & 0x0000FFFF);
    hdr->cls = (u8)(r2 & 0xFF000000);
    hdr->subcls = (u8)(r2 & 0x00FF0000);
    hdr->progif = (u8)(r2 & 0x0000FF00);
    hdr->revid = (u8)(r2 & 0x000000FF);
    hdr->bist = (u8)(r3 & 0xFF000000);
    hdr->hdrt = (u8)(r3 & 0x00FF0000);
    hdr->lattmr = (u8)(r3 & 0x0000FF00);
    hdr->cachesz = (u8)(r3 & 0x000000FF);
}