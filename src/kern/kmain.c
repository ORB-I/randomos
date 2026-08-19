#include <core/mem/vmm.h>
#include <core/mem/pmm.h>
#include <core/std.h>
#include <core/limreqs.h>
#include <core/panic.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/printf.h>
#include <core/fpu.h>

#include <lib/sh.h>
#include <lib/loader.h>
#include <lib/syscall.h>

#include <drivers/gettimeofday.h>
#include <drivers/kbd.h>
#include <drivers/rtc.h>
#include <drivers/pic.h>
#include <drivers/mouse.h>
#include <drivers/apic.h>
#include <drivers/acpi.h>
#include <drivers/term.h>
#include <drivers/timer.h>
#include <drivers/tsc.h>
#include <drivers/ata.h>
#include <drivers/ff16_init.h>
#include <drivers/fs.h>
#include <drivers/fb.h>
#include <drivers/usb/uhci.h>

#include <lai/helpers/pm.h>
#include <ff16/ff.h>

u64 ram_max = 0;
extern void gdt_init();
extern void sci_hdlr();
core_acpi_t* acpi_hdl = NULL;

void kmain() {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        for (;;) asm("hlt");
    }

    if (!hhdm_request.response || !mmap_req.response ||
        !rsdp_req.response || !kaddr_req.response) {
        for (;;) asm("hlt");
    }

    // Top of physical RAM, derived from the memmap.
    for (usize i = 0; i < mmap_req.response->entry_count; i++) {
        struct limine_memmap_entry* e = mmap_req.response->entries[i];
        if (e->base + e->length > ram_max) {
            ram_max = e->base + e->length;
        }
    }

    gdt_init();
}

void init_allterm() {
    if (!fb_req.response || fb_req.response->framebuffer_count == 0) {
        for (;;) asm("hlt");
    }

    if (init_fbdrv(fb_req.response->framebuffers[0]) < 0) {
        for (;;) asm("hlt");
    }

    int termfb = create_fb(FBTYPE_TERM);
    if (termfb < 0) {
        for (;;) asm("hlt");
    }

    if (create_fb(FBTYPE_GUI) < 0) {
        for (;;) asm("hlt");
    }

    if (init_term(termfb) < 0) {
        for (;;) asm("hlt");
    }

    if (switch_fb(termfb) < 0) {
        for (;;) asm("hlt");
    }

    term_clear();
}

void kmain_aftergdt() {
    init_fpu();
    init_tsc();
    
    pmm_init();
    vmm_init();

    init_allterm();

    asm("cli");
    pic_remap(0x20, 0x28);
    pic_disable();

    idt_init();

    core_acpi_t acpi;
    acpi_hdl = &acpi;
    init_acpi(&acpi);

    printf("IO: Initializing APIC & IOAPIC\n");
    apic_init();

    // Register the SCI interrupt now that apic_init() has built the
    // IOAPIC redirection table.
    init_irq(acpi.fadt->sci_int, sci_hdlr);

    asm("sti");

    init_gettimeofday();

    int drive = ata_init();
    if (drive > 0) {
        ff16_set_drive(drive);
        if (mount("", MNT_FORMAT) < 0) {
            printf("failed to mount\n");
        }
    } else {
        printf("KERN: No drive available\n");
    }

    int kbtype = KBD_USBHID;
    int mbtype = MOUSE_USBHID;
    if (init_uhci() < 0) {
        kbtype = KBD_PS2;
        mbtype = MOUSE_PS2;
    }

    init_syscalls();

    printf("IO: Requesting keyboard type %d\n", kbtype);
    init_kbd(kbtype);
    printf("IO: Requesting mouse type %d\n", mbtype);
    init_mouse(mbtype);

    sh();
    for (;;);
}
