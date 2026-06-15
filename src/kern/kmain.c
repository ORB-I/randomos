#include <core/std.h>
#include <core/multiboot.h>
#include <core/panic.h>
#include <core/asmh.h>
#include <core/idt.h>
#include <core/mem.h>

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

void kmain(u32 mag, multiboot_info_t* mbinfo, u8* ebda) {
    vga_clear();
    vga_setcolor(VGA_LIGHT_GREEN);

    if (mag != MULTIBOOT_BOOTLOADER_MAGIC) panic("INVALID MULTIBOOT BOOTLOADER MAGIC");

    pmem_init(mbinfo);

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
    init_acpi(&acpi, ebda);

    bool hasdrv = true;

    int drive = ata_init();
    if (drive > 0) {
        ff16_set_drive(drive);
        if (mount("", MNT_FORMAT) < 0) {
            printf("failed to mount\n");
        } else {
            hasdrv = true;
            int fd = open("test", O_CREAT);
            close(fd);
        }
    } else {
        hasdrv = false;
        printf("KERN: No drive available\n");
    }

    init_syscalls();

    printf("IO: Initializing and enabling keyboard\n");
    init_kbd();
    enable_kbd();

    sh(hasdrv);
    for (;;);
}