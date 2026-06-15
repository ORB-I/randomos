#include <core/asmh.h>

#include <drivers/acpi.h>

s32 acpi_write8(genaddr_t* addr, u8 val) {
    if (addr->accsz < ACCESS8 && addr->bitwdth < 8) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *((u32*)addr->addr) = val;
            return 0;
        case ADDRSPACE_IO:
            outb(addr->addr, val);
            return 0;
        default:
            return -1;
    }
}

s32 acpi_write16(genaddr_t* addr, u16 val) {
    if (addr->accsz < ACCESS16 && addr->bitwdth < 16) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *((u32*)addr->addr) = val;
            return 0;
        case ADDRSPACE_IO:
            outb(addr->addr, val);
            return 0;
        default:
            return -1;
    }
}

s32 acpi_write32(genaddr_t* addr, u32 val) {
    if (addr->accsz < ACCESS32 && addr->bitwdth < 32) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *((u32*)addr->addr) = val;
            return 0;
        case ADDRSPACE_IO:
            outb(addr->addr, val);
            return 0;
        default:
            return -1;
    }
}

s32 acpi_read8(genaddr_t* addr, u8* out) {
    if (addr->accsz < ACCESS8 && addr->bitwdth < 8) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *out = *((u32*)addr->addr);
            return 0;
        case ADDRSPACE_IO:
            *out = inb(addr->addr);
            return 0;
        default:
            return -1;
    }
}

s32 acpi_read16(genaddr_t* addr, u16* out) {
    if (addr->accsz < ACCESS16 && addr->bitwdth < 16) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *out = *((u32*)addr->addr);
            return 0;
        case ADDRSPACE_IO:
            *out = inb(addr->addr);
            return 0;
        default:
            return -1;
    }
}

s32 acpi_read32(genaddr_t* addr, u32* out) {
    if (addr->accsz < ACCESS32 && addr->bitwdth < 32) return -1;
    switch (addr->space) {
        case ADDRSPACE_SYS:
            *out = *((u32*)addr->addr);
            return 0;
        case ADDRSPACE_IO:
            *out = inb(addr->addr);
            return 0;
        default:
            return -1;
    }
}