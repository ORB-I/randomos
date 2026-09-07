#include <core/mem/vmm.h>
#include <core/panic.h>
#include <core/std.h>
#include <core/kprint.h>

#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/event.h>
#include <uacpi/acpi.h>
#include <drivers/nacpi.h>

struct acpi_fadt *gfadt;

/*
 * Early phase: tables + registers only. Runs before apic_init() so the
 * APIC/HPET/SMP drivers can use uACPI's table API. Does not touch the
 * namespace and installs no interrupt handlers.
 */
void init_acpi() {
    uacpi_status ret = uacpi_initialize(0);
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_initialize error: %s", uacpi_status_to_string(ret));
    }

    kprint("ACPI: tables initialized\n");
}

/*
 * Late phase: namespace load + initialize, GPE finalization, FADT grab.
 *
 * uACPI installs the SCI interrupt handler at the end of
 * uacpi_namespace_load(), which requires the IOAPIC redirection table
 * to exist, so this must run after apic_init().
 */
void init_acpi_ns() {
    uacpi_status ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
    }

    // evaluates \_PIC, so it needs the namespace loaded
    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        panic("uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        panic("uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
    }

    ret = uacpi_table_fadt(&gfadt);
    if (uacpi_unlikely_error(ret)) {
        panic("uACPI Failed to retrieve FADT: %s", uacpi_status_to_string(ret));
    }

    kprint("ACPI: uACPI ready (SCI IRQ %u)\n", gfadt->sci_int);
}