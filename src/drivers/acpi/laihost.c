#include "core/mem/vmm.h"
#include <core/panic.h>
#include <core/liballoc.h>
#include <core/asmh.h>

#include <lib/string.h>

#include <drivers/term.h>
#include <drivers/acpi.h>
#include <drivers/rtc.h>
#include <drivers/pci.h>

#include <lai/host.h>

s32 vchksum(sdt_header_t* hdr);

core_acpi_t* __lai_core_acpi__;
void set_lai_acpi(core_acpi_t* acpi) { __lai_core_acpi__ = acpi; }

void laihost_log(int _, const char* msg) { printf("ACPI: %s\n", msg); }
__attribute__((noreturn)) void laihost_panic(const char* msg) { panic(msg); }

void* laihost_malloc(size_t sz) { return malloc(sz); }
void* laihost_realloc(void* ptr, size_t newsize, size_t _) { return realloc(ptr, newsize); }
void laihost_free(void* ptr, size_t _) { free(ptr); }

void laihost_outb(u16 port, u8 val) { return outb(port, val); };
void laihost_outw(u16 port, u16 val) { return outw(port, val); };
void laihost_outd(u16 port, u32 val) { return outl(port, val); }

u8 laihost_inb(u16 port) { return inb(port); }
u16 laihost_inw(u16 port) { return inw(port); }
u32 laihost_ind(u16 port) { return inl(port); }

u8 laihost_pci_readb(u16 seg, u8 bus, u8 slot, u8 fn, u16 off) {
    (void)seg;
    return pci_cfg_inb(bus, slot, fn, off);
}

u16 laihost_pci_readw(u16 seg, u8 bus, u8 slot, u8 fn, u16 off) {
    (void)seg;
    return pci_cfg_inw(bus, slot, fn, off);
}

u32 laihost_pci_readd(u16 seg, u8 bus, u8 slot, u8 fn, u16 off) {
    (void)seg;
    return pci_cfg_inl(bus, slot, fn, off);
}

void laihost_pci_writeb(u16 seg, u8 bus, u8 slot, u8 fn,  u16 off, u8 val) {
    (void)seg;
    pci_cfg_outb(bus, slot, fn, off, val);
}

void laihost_pci_writew(u16 seg, u8 bus, u8 slot, u8 fn, u16 off, u16 val) {
    (void)seg;
    pci_cfg_outw(bus, slot, fn, off, val);
}

void laihost_pci_writed(u16 seg, u8 bus, u8 slot, u8 fn, u16 off, uint32_t val) {
    (void)seg;
    pci_cfg_outl(bus, slot, fn, off, val);
}

void laihost_sleep(u64 ms) { rtc_sleep(ms / 1000); }

void* laihost_map(uintptr_t phys_addr, size_t _) {  return (void*)(phys_addr + HHDM_START); }
void laihost_unmap(void* _, size_t __) { (void)__; }

void* laihost_scan(const char *sig, size_t index) {
    if (__lai_core_acpi__->xsdt == NULL && __lai_core_acpi__->rsdt == NULL) panic("No RSDT/XSDT found");

    if (strneq((char*)sig, "RSD PTR ", 8)) {
        return (void *)__lai_core_acpi__->rsdp;
    }

    if (strneq((char*)sig, "DSDT", 4)) {
        if (__lai_core_acpi__->xsdt != NULL) {
            return (void*)(__lai_core_acpi__->fadt->xdsdt + HHDM_START);
        } else {
            return (void*)(__lai_core_acpi__->fadt->dsdt + HHDM_START);
        }
    }

    size_t mcnt = 0;
    for (u32 i = 0; i < sdt_entries(__lai_core_acpi__); i++) {
        sdt_header_t* hdr;
        if (__lai_core_acpi__->xsdt != NULL) {
            hdr = (sdt_header_t*)__lai_core_acpi__->xsdt->entries[i];
        } else {
            hdr = (sdt_header_t*)(((u64)__lai_core_acpi__->rsdt->entries[i]) + HHDM_START);
        }

        if (strneq(hdr->sig, (char*)sig, 4) && vchksum(hdr)) {
            if (mcnt == index) {
                return (void*)hdr;
            }
            mcnt++;
        }
    }

    return NULL;
}