#include <core/limine.h>
#include <core/std.h>
#include <core/limreqs.h>

#define LIMINE_REQ __attribute__((used, section(".limine_requests")))

__attribute__((used, section(".limine_requests_start")))
static volatile u64 _limreq_sm[] = LIMINE_REQUESTS_START_MARKER;

LIMINE_REQ volatile u64 limine_base_revision[] = LIMINE_BASE_REVISION(6);
LIMINE_REQ volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

LIMINE_REQ volatile struct limine_memmap_request mmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

LIMINE_REQ volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

LIMINE_REQ volatile struct limine_rsdp_request rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

LIMINE_REQ volatile struct limine_executable_address_request kaddr_req = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile u64 _limreq_em[] = LIMINE_REQUESTS_END_MARKER;