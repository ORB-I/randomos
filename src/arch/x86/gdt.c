#include <core/std.h>

struct gdtr {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct gdt_entry {
    u16 limlow;
    u16 base_low;
    u8 base_mid;
    u8 acc;
    u8 gran;
    u8 base_high;
} __attribute__((packed));

struct tss_entry {  
    u32 prev_tss; u32 esp0; u32 ss0; u32 esp1;
    u32 ss1; u32 esp2; u32 ss2; u32 cr3;
    u32 eip; u32 eflags; u32 eax; u32 ecx; 
    u32 edx; u32 ebx; u32 esp; u32 ebp;
    u32 esi; u32 edi; u32 es; u32 cs;
    u32 ss; u32 ds; u32 fs; u32 gs;
    u32 ldt; u16 trap; u16 iomap_base;
} __attribute__((packed));

struct gdt_entry _gdt[6];
struct gdtr _gdtr;
struct tss_entry _tss;

extern u8 *kern_stack;

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

#define MiB (1024 * 1024)

void load_gdt() {
    asm volatile("cli");
    set_gdtent(NULLSS, 0, 0, 0, 0);
    set_gdtent(KCSS, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    set_gdtent(KDSS, 0, 0xFFFFFFFF, 0x92, 0xCF);
    set_gdtent(UCSS, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    set_gdtent(UDSS, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    u8 *tss_ptr = (u8 *)&_tss;
    for (u32 i = 0; i < sizeof(struct tss_entry); i++) {
        tss_ptr[i] = 0;
    }

    _tss.ss0 = 0x10;
    _tss.esp0 = (u32)(kern_stack + 4096);
    _tss.iomap_base = sizeof(struct tss_entry);

    u32 tss_base = (u32)&_tss;
    u32 tss_limit = sizeof(struct tss_entry) - 1;
    set_gdtent(TSS, tss_base, tss_limit, 0x89, 0x00);

    _gdtr.limit = sizeof(_gdt) - 1;
    _gdtr.base = (u32)_gdt;

    asm volatile(
        "lgdt %0\n\t"
        "ljmp $0x08, $.flush\n\t"
        ".flush:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"

        "ltr %1\n\t" 
        ::"m"(_gdtr), "r"((uint16_t)(sizeof(struct gdt_entry) * TSS))
        : "eax", "memory"
    );

    asm volatile("sti");
}