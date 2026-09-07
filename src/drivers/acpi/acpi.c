//#include "core/errno.h"
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <core/panic.h>
#include <core/std.h>
#include <core/limreqs.h>
#include <core/kprint.h>

#include <lib/string.h>

//#include <drivers/acpi.h>
#include <drivers/pic.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/event.h>
#include <uacpi/acpi.h>
#include <drivers/nacpi.h>

//#include <lai/core.h>
//#include <lai/helpers/sci.h>
//#include <lai/core.h>

/*s32 is_rsdp(char* sig) {
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
    // If the 64-bit PM1 control block is not available, fall back to
    // the legacy 32-bit one.
    if (acpi->fadt->xpm1a_ctrl_block.addr == 0 ||
        acpi->fadt->xpm1a_ctrl_block.accsz == 0) {
            return (inw(acpi->fadt->pm1a_ctrl_block) & 1) != 0;
    }

    u32 out;
    acpi_read32(&acpi->fadt->xpm1a_ctrl_block, &out);
    return (out & 1) != 0;
}*/

struct acpi_fadt* gfadt;
s32 acpi_sci_irqno;
void init_acpi(/*core_acpi_t* acpi*/) {
    /*acpi->rsdp = xlate_limptr(rsdp_req.response->address);
    if (!acpi->rsdp) panic("CANNOT LOCATE VALID RSDP");

    kprint("ACPI: Loading LAI AML interpreter (RSDP Rev: %d)\n", acpi->rsdp->rev);

    if (acpi->rsdp->rev >= 2 && acpi->rsdp->xsdt_addr != 0) {
        acpi->xsdt = (xsdt_t*)(acpi->rsdp->xsdt_addr + HHDM_START);
        if (strneq(acpi->xsdt->hdr.sig, "XSDT", 4)) {
            acpi->fadt = find_acpitbl(acpi->xsdt, "FACP");
        }
        acpi->rsdt = NULL;
    }

    if (!acpi->fadt) {
        if (!acpi->rsdp->rsdt_addr) panic("BOTH RSDT AND XSDT ARE NULL");

        acpi->rsdt = (rsdt_t*)((u64)acpi->rsdp->rsdt_addr + HHDM_START);
        if (!strneq(acpi->rsdt->hdr.sig, "RSDT", 4)) {
            panic("RSDT SIGNATURE INVALID: Got %4s", acpi->rsdt->hdr.sig);
        }

        acpi->fadt = find_acpitbl_32(acpi->rsdt, "FACP");
        acpi->xsdt = NULL;
    }

    if (!acpi->fadt) panic("CANNOT FIND FADT (FACP) TABLE");

    acpi_sci_irqno = acpi->fadt->sci_int;
    // NOTE: SCI IRQ routing is set up later in kmain_aftergdt(), once
    // apic_init() has built the IOAPIC redirection table.
    set_lai_acpi(acpi);

    kprint("Setting ACPI Revision\n");
    lai_set_acpi_revision(acpi->rsdp->rev);
    lai_enable_tracing(LAI_TRACE_OP | LAI_TRACE_NS | LAI_TRACE_IO);
    kprint("Creating ACPI Namespace\n");
    lai_create_namespace();
    kprint("Enabling ACPI\n");
    lai_enable_acpi(0);
    kprint("ACPI initialization complete\n");*/

    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_initialize error: %s", uacpi_status_to_string(ret));
    }

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
    }

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
    }

    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        panic("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
    }



    ret = uacpi_table_fadt(&gfadt);
    if (uacpi_unlikely_error(ret)) {
        panic("uACPI Failed to retrieve FADT: %s", uacpi_status_to_string(ret));
    }
}

void c_sci_hdlr() {
    pic_send_eoi(acpi_sci_irqno);
}