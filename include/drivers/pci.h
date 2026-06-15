#pragma once
#include <core/std.h>

typedef struct {
    u16 devid;
    u16 vndid;
    u16 stat;
    u16 cmd;
    u8 cls;
    u8 subcls;
    u8 progif;
    u8 revid;
    u8 bist;
    u8 hdrt;
    u8 lattmr;
    u8 cachesz;
} pci_chdr_t;

u8 pci_cfg_inb(u8 bus, u8 slot, u8 fn, u8 off);
u16 pci_cfg_inw(u8 bus, u8 slot, u8 fn, u8 off);
u32 pci_cfg_inl(u8 bus, u8 slot, u8 fn, u8 off);

void pci_cfg_outb(u8 bus, u8 slot, u8 fn, u8 off, u8 val);
void pci_cfg_outw(u8 bus, u8 slot, u8 fn, u8 off, u16 val);
void pci_cfg_outl(u8 bus, u8 slot, u8 fn, u8 off, u32 val);

void pci_get_chdr(u32 bus, u32 slot, pci_chdr_t* hdr);