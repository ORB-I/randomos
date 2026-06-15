#include <core/multiboot.h>

#define FLAGS (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)

__attribute__((section(".multiboot"), aligned(8)))
const struct multiboot_header __mb_header = {
    .magic = MULTIBOOT_HEADER_MAGIC,
    .flags = FLAGS,
    .checksum = -(MULTIBOOT_HEADER_MAGIC + FLAGS)
};