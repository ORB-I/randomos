#pragma once
#include <core/std.h>
#include <core/limine.h>

extern volatile u64 limine_base_revision[];
extern volatile struct limine_framebuffer_request framebuf_req;
extern volatile struct limine_memmap_request mmap_req;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_rsdp_request rsdp_req;