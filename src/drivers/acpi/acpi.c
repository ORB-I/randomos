#include "core/mem/vmm.h"
#include <core/asmh.h>
#include <core/panic.h>
#include <core/std.h>
#include <core/limreqs.h>

#include <lib/string.h>

#include <drivers/acpi.h>
#include <drivers/pic.h>
#include <drivers/rtc.h>
#include <drivers/term.h>

#include <lai/core.h>
#include <lai/helpers/sci.h>

extern void sci_hdlr();

s32 is_rsdp(char* sig) {
    return strneq(sig, "RSD PTR ", 8);
}

s32 rsdp_chksum(u8* rsdp_addr) {
    u8 sum = 0;
    for (s32 i = 0; i < 20; i++) sum += rsdp_addr[i];
    return (sum == 0);
}

s32 vchksum(sdt_header_t* hdr) {
    u8 sum = 0;
    for (usize i = 0; i < hdr->len; i++) 
        sum += ((u8 *) hdr)[i];
    return sum == 0;
}

void* find_acpitbl(xsdt_t* xsdt, char name[4]) {
    u32 entries = (xsdt->hdr.len - sizeof(sdt_header_t)) / 8;
    for (u32 i = 0; i < entries; i++) {
        sdt_header_t* hdr = (sdt_header_t*)(xsdt->entries[i] + HHDM_START);
        if (strneq(hdr->sig, name, 4) && vchksum(hdr)) {
            return (void*)hdr;
        }
    }
    return NULL;
}

void* find_acpitbl_32(rsdt_t* rsdt, char name[4]) {
    u32 entries = (rsdt->hdr.len - sizeof(sdt_header_t)) / 4;
    for (u32 i = 0; i < entries; i++) {
        sdt_header_t* hdr = (sdt_header_t*)((u64)rsdt->entries[i] + HHDM_START);
        if (strneq(hdr->sig, name, 4) && vchksum(hdr)) {
            return (void*)hdr;
        }
    }
    return NULL;
}

s32 acpi_ready(core_acpi_t* acpi) {
    if (acpi->fadt->xpm1a_ctrl_block.addr != 0 ||
        acpi->fadt->xpm1a_ctrl_block.accsz == 0) {
            return (inw(acpi->fadt->pm1a_ctrl_block) & 1) != 0;
    }

    u32 out;
    acpi_read32(&acpi->fadt->xpm1a_ctrl_block, &out);
    return (out & 1) != 0;
}

void init_acpi(core_acpi_t* acpi) {
    acpi->rsdp = xlate_limptr(rsdp_req.response->address);
    if (!acpi->rsdp) panic("CANNOT LOCATE VALID RSDP");

    printf("ACPI: Loading LAI AML interpreter (RSDP Rev: %d)\n", acpi->rsdp->rev);

    if (acpi->rsdp->rev >= 2 && acpi->rsdp->xsdt_addr != 0) {
        acpi->xsdt = (xsdt_t*)(acpi->rsdp->xsdt_addr + HHDM_START);
        if (strneq(acpi->xsdt->hdr.sig, "XSDT", 4)) {
            acpi->fadt = find_acpitbl(acpi->xsdt, "FACP");
        }
    }
    
    if (!acpi->fadt) {
        if (!acpi->rsdp->rsdt_addr) panic("BOTH RSDT AND XSDT ARE NULL");
        
        acpi->rsdt = (rsdt_t*)((u64)acpi->rsdp->rsdt_addr + HHDM_START);
        if (!strneq(acpi->rsdt->hdr.sig, "RSDT", 4)) {
            panic("RSDT SIGNATURE INVALID: Got %04s", acpi->rsdt->hdr.sig);
        }
        
        acpi->fadt = find_acpitbl_32(acpi->rsdt, "FACP");
    }

    if (!acpi->fadt) panic("CANNOT FIND FADT (FACP) TABLE");

    init_irq(acpi->fadt->sci_int, sci_hdlr);
    set_lai_acpi(acpi);

    lai_set_acpi_revision(acpi->rsdp->rev);
    lai_create_namespace();
    lai_enable_acpi(0);
}

void c_sci_hdlr() {}