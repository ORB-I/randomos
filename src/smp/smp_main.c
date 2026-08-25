#include <core/std.h>
#include <drivers/display/serial.h>
#include <core/printf.h>

void smp_main(u64 acpi_id) {
    serial_printf("SMP %llu active\n", acpi_id);
    for (;;) {
        asm volatile("hlt");
    }
}