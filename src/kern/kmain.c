#include <core/mem/vmm.h>
#include <core/mem/pmm.h>
#include <core/std.h>
#include <core/limreqs.h>
#include <core/panic.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/printf.h>
#include <core/fpu.h>

#include <lib/loader.h>
#include <lib/syscall.h>
#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <smp/smp.h>

#include <drivers/time/gettimeofday.h>
#include <drivers/hid/kbd.h>
#include <drivers/time/rtc.h>
#include <drivers/pic.h>
#include <drivers/hid/mouse.h>
#include <drivers/apic.h>
#include <drivers/acpi.h>
#include <drivers/display/term.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/block.h>
#include <drivers/display/fb.h>
#include <drivers/usb/uhci.h>
#include <drivers/net/e1000.h>
#include <drivers/net/virtio_net.h>
#include <drivers/rng/virtio_rng.h>
#include <drivers/time/clock.h>
#include <smp/ap.h>

#include <lai/helpers/pm.h>

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

    if (init_term(termfb) < 0) {
        for (;;) asm("hlt");
    }

    if (switch_fb(termfb) < 0) {
        for (;;) asm("hlt");
    }

    term_clear();
}

int try_init(const char* path) {
    char* argv[] = {(char*)path, NULL};
    char* envp[] = {NULL};
    if (new_process(path, argv, envp, 0) < 0) {
        return 0;
    }
    return 1;
}

void ap_testtask() {
    serial_printf("Hello from SMP%d\n", get_apicid());
}

int init_lwip();
void kmain_aftergdt() {
    init_fpu();
    if (init_clock(CLOCK_TSC) < 0) {
        for (;;) asm("hlt"); // init_clock will try hpet first but fallback to tsc if necessary
                             // but we do need a clock to function
    }

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

    if (init_clock(CLOCK_HPET) < 0) {
        printf("Switch to HPET failed\n");
    }
    asm("sti");

    init_gettimeofday();
    init_cores();

    if (block_init() != BLOCK_OK) {
        panic("KERN: No drive available\n");
    }

    if (fs_probe_mount() < 0) {
        panic("Failed to mount\n");
    }

    int kbtype = KBD_USBHID;
    int mbtype = MOUSE_USBHID;
    if (init_uhci() < 0) {
        kbtype = KBD_PS2;
        mbtype = MOUSE_PS2;
    }

    virtio_rng_init();
    rng_init();
    virtio_net_init();
    e1000_init();
    init_lwip();

    init_syscalls();

    printf("IO: Requesting keyboard type %d\n", kbtype);
    init_kbd(kbtype);
    printf("IO: Requesting mouse type %d\n", mbtype);
    init_mouse(mbtype);

    printf("Testing AP\n");
    while (ap_run(ap_testtask, NULL) < 0);

    if (init_scheduler() < 0) panic("Failed to initialize scheduler\n");

    char* argv[] = {"/bin/init", NULL};
    char* envp[] = {NULL};
    if (new_process("/bin/init", argv, envp, 0) < 0) {
        panic("init failed");
    }

    start_scheduler();
}
