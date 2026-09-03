#include <core/std.h>
#include <lib/string.h>
#include <arch/gdt.h>

struct gdt_entry _gdt[8]; 
struct gdtr _gdtr;
struct tss_entry _tss;

#define STACK_SIZE 65536
__align(16) u8 kern_stack[STACK_SIZE];
__align(16) u8 intr_stack[STACK_SIZE];
__align(16) u8 exct_stack[STACK_SIZE];

void set_gdtent(struct gdt_entry* gdt, int n, u32 base, u32 lim, u8 acc, u8 gran) {
    struct gdt_entry *e = &gdt[n];

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

void set_gdt_tss(struct gdt_entry* gdt, int n, u64 base, u32 lim, u8 acc) {
    struct gdt_tss_entry *t = (struct gdt_tss_entry *)&gdt[n];

    t->limit_low = (lim & 0xFFFF);
    t->base_low = (base & 0xFFFF);
    t->base_mid1 = (base >> 16) & 0xFF;
    t->access = acc;
    t->granularity = ((lim >> 16) & 0x0F);
    t->base_mid2 = (base >> 24) & 0xFF;
    t->base_high = (base >> 32) & 0xFFFFFFFF;
    t->reserved = 0;
}

void reset_rsp(u64 addr) {
    _tss.rsp0 = addr;
}

void kmain_aftergdt();
void gdt_init() {
    asm volatile("cli");

    set_gdtent(_gdt, NULLSS, 0, 0, 0, 0);
    set_gdtent(_gdt, KCSS, 0, 0xFFFFFFFF, 0x9A, 0x20);
    set_gdtent(_gdt, KDSS, 0, 0xFFFFFFFF, 0x92, 0x00);
    set_gdtent(_gdt, UCSS, 0, 0xFFFFFFFF, 0xFA, 0x20);
    set_gdtent(_gdt, UDSS, 0, 0xFFFFFFFF, 0xF2, 0x00);

    memset(&_tss, 0, sizeof(_tss));

    _tss.rsp0 = (u64)(kern_stack + sizeof(kern_stack));
    _tss.iomap_base = sizeof(struct tss_entry);

    _tss.ist[0] = (u64)(intr_stack + sizeof(intr_stack));
    _tss.ist[1] = (u64)(exct_stack + sizeof(exct_stack));

    u64 tss_base = (u64)&_tss;
    u32 tss_limit = sizeof(struct tss_entry) - 1;
    set_gdt_tss(_gdt, TSS, tss_base, tss_limit, 0x89);

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

        "mov %1, %%rsp\n\t"

        "sti\n\t"
        // clangs integrated assembler needs the star for indirect
        // calls, gas alone accepts this without it
        "call *%2"
        ::"m"(_gdtr), "r"(kern_stack + sizeof(kern_stack)), "r"(kmain_aftergdt)
        : "rax", "memory"
    );

    asm volatile("sti");
}