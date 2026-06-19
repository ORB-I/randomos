#include <core/mem/vmm.h>
#include <core/mem/pmm.h>
#include <core/std.h>
#include <core/limreqs.h>
#include <core/panic.h>
#include <core/asmh.h>
#include <core/idt.h>

#include <lib/sh.h>
#include <lib/loader.h>

#include <drivers/kbd.h>
#include <drivers/rtc.h>
#include <drivers/pic.h>
#include <drivers/acpi.h>
#include <drivers/vga.h>
#include <drivers/timer.h>
#include <drivers/ata.h>
#include <drivers/ff16_init.h>
#include <drivers/fs.h>

#include <lai/helpers/pm.h>
#include <ff16/ff.h>

u64 ram_max = 0;
extern void gdt_init();

void kmain() {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        for (;;) { asm("hlt"); }
    }

    if (!hhdm_request.response || !mmap_req.response) {
        for (;;) { asm("hlt"); }
    }

    gdt_init();
    pmm_init();
    vmm_init();

    
    vga_init();
    vga_clear();
    vga_setdflcolor(VGA_LIGHT_GREEN);
    vga_clearcolor();

    printf("Hello!\n");

    asm("cli");
    printf("IO: Initializing PIC\n");
    pic_remap(0x20, 0x28);
    
    printf("IO: Shutting down ALL IO\n");
    pic_disable();

    idt_init();

    printf("IO: Initializing and enabling clock RTC\n");
    init_rtc();
    enable_rtc();
    asm("sti");

    core_acpi_t acpi;
    init_acpi(&acpi);

    int drive = ata_init();
    if (drive > 0) {
        ff16_set_drive(drive);
        if (mount("", MNT_FORMAT) < 0) {
            printf("failed to mount\n");
        }
    } else {
        printf("KERN: No drive available\n");
    }

    //init_syscalls();

    //printf("IO: Initializing and enabling keyboard\n");
    //init_kbd();
    //enable_kbd();

    //sh();
    for (;;);
}