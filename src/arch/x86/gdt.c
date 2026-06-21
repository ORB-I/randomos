#include <core/std.h>
#include <lib/string.h>

struct gdtr {
    u16 limit;
    u64 base;
} __attribute__((packed));

struct gdt_entry {
    u16 limlow;
    u16 base_low;
    u8 base_mid;
    u8 acc;
    u8 gran;
    u8 base_high;
} __attribute__((packed));

struct gdt_tss_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid1;
    u8  access;
    u8  granularity;
    u8  base_mid2;
    u32 base_high;
    u32 reserved;
} __attribute__((packed));

struct tss_entry {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist[7];
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed));


struct gdt_entry _gdt[7]; 
struct gdtr _gdtr;
struct tss_entry _tss;

__attribute__((aligned(16))) u8 kern_stack[16384];

#define NULLSS 0
#define KCSS 1
#define KDSS 2
#define UCSS 3
#define UDSS 4
#define TSS 5

void set_gdtent(int n, u32 base, u32 lim, u8 acc, u8 gran) {
    struct gdt_entry *e = &_gdt[n];

    e->base_low = (base & 0xFFFF);
    e->base_mid = (base >> 16) & 0xFF;
    e->base_high = (base >> 24) & 0xFF;

    if (lim > 65535) {
        lim = lim >> 12;
    }

    e->limlow = (lim & 0xFFFF);
    e->gran = (gran & 0xF0) | ((lim >> 16) & 0x0F);
    e->acc = acc;
}

void set_gdt_tss(int n, u64 base, u32 lim, u8 acc) {
    struct gdt_tss_entry *t = (struct gdt_tss_entry *)&_gdt[n];

    t->limit_low = (lim & 0xFFFF);
    t->base_low = (base & 0xFFFF);
    t->base_mid1 = (base >> 16) & 0xFF;
    t->access = acc;
    t->granularity = ((lim >> 16) & 0x0F);
    t->base_mid2 = (base >> 24) & 0xFF;
    t->base_high = (base >> 32) & 0xFFFFFFFF;
    t->reserved = 0;
}

void gdt_init() {
    asm volatile("cli");

    set_gdtent(NULLSS, 0, 0, 0, 0);
    set_gdtent(KCSS, 0, 0xFFFFFFFF, 0x9A, 0x20);
    set_gdtent(KDSS, 0, 0xFFFFFFFF, 0x92, 0x00);
    set_gdtent(UCSS, 0, 0xFFFFFFFF, 0xFA, 0x20);
    set_gdtent(UDSS, 0, 0xFFFFFFFF, 0xF2, 0x00);

    memset(&_tss, 0, sizeof(_tss));

    _tss.rsp0 = (u64)(kern_stack + sizeof(kern_stack));
    _tss.iomap_base = sizeof(struct tss_entry);

    u64 tss_base = (u64)&_tss;
    u32 tss_limit = sizeof(struct tss_entry) - 1;
    set_gdt_tss(TSS, tss_base, tss_limit, 0x89);

    _gdtr.limit = sizeof(_gdt) - 1;
    _gdtr.base = (u64)_gdt;

    asm volatile(
        "lgdt %0\n\t"
        
        "pushq $0x08\n\t"
        "leaq .flush(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"

        ".flush:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"

        "mov $0x28, %%ax\n\t"
        "ltr %%ax\n\t"

        "mov %1, %%rsp"
        ::"m"(_gdtr), "r"(kern_stack + sizeof(kern_stack))
        : "rax", "memory"
    );

    asm volatile("sti");
}