#include <core/asmh.h>
#include <core/panic.h>
#include <core/std.h>

#include <lib/string.h>

#include <drivers/acpi.h>
#include <drivers/pic.h>
#include <drivers/rtc.h>
#include <drivers/vga.h>

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

void* find_acpitbl(rsdt_t* rsdt, char name[4]) {
    for (u32 i = 0; i < rsdt_entries(rsdt); i++) {
        sdt_header_t* hdr = (sdt_header_t*)rsdt->entries[i];
        if (strneq(hdr->sig, name, 4) && vchksum(hdr)) return (void*)rsdt->entries[i];
    }
    return NULL;
}

rsdp_t* try_find_rsdp(const u8* base, u8* max) {
    for (; base < max; base += 16) {
        if (is_rsdp((char*)base) && rsdp_chksum((u8*)base)) {
            return (rsdp_t*)base;
        }
    }
    return NULL;
}

rsdp_t* locate_rsdp(u8* ebda) {
    if (ebda) {
        rsdp_t* edba_rsdp = try_find_rsdp(ebda, ebda + 1024);
        if (edba_rsdp) return edba_rsdp;
    }

    return try_find_rsdp((u8*)0x0E0000, (u8*)0x100000);
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

void init_acpi(core_acpi_t* acpi, u8* ebda) {
    printf("ACPI: Preparing to load LAI AML interpreter\n");
    acpi->rsdp = locate_rsdp(ebda);
    if (!acpi->rsdp) panic("CANNOT LOCATE VALID RSDP");
    if (!acpi->rsdp->rsdt_addr) panic("NO RSDT FOUND");

    acpi->rsdt = (rsdt_t*)acpi->rsdp->rsdt_addr;
    if (!strneq(acpi->rsdt->hdr.sig, "RSDT", 4)) panic("RSDT INVALID");
    acpi->fadt = find_acpitbl(acpi->rsdt, "FACP");
    if (!acpi->fadt) panic("CANNOT FIND FADT");

    init_irq(acpi->fadt->sci_int, sci_hdlr);
    
    if (acpi->fadt->smi_port == 0 || (acpi->fadt->acpi_dis == 0 && acpi->fadt->acpi_en == 0) || acpi_ready(acpi)) return;

    outb(acpi->fadt->smi_port, acpi->fadt->acpi_en);
    asm volatile("sti");

    rtc_sleep(3);

    while (!acpi_ready(acpi));

    set_lai_acpi(acpi);

    lai_set_acpi_revision(acpi->rsdp->rev);
    lai_create_namespace();
}

void c_sci_hdlr() {
    
}