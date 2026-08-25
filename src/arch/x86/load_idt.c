#include <core/std.h>
#include <core/printf.h>
#include <arch/idt.h>

static idt_entry_t _idt[IDT_SIZE];
static idtr_t _idtr;

void idt_regintr(idt_entry_t* idt, u8 vector, void* isr, u8 flags, int ist) {
    if (!idt) idt = _idt;
    idt_entry_t* dsc = &idt[vector];
    u64 addr = (u64)isr;

    dsc->isr_low = addr & 0xFFFF;
    dsc->kernel_cs = 0x08;
    dsc->ist = ist;
    dsc->attributes = flags;
    dsc->isr_mid = (addr >> 16) & 0xFFFF;
    dsc->isr_high = (addr >> 32) & 0xFFFFFFFF;
    dsc->reserved = 0;
}

extern void (*int_hdlr_table[])();

void idt_init() {
    printf("IO: Initializing IDT\n");
    s32 vectors[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19};
    s32 count = sizeof(vectors) / sizeof(vectors[0]);

    for (s32 i = 0; i < count; i++) {
        s32 vec = vectors[i];
        if (int_hdlr_table[vec]) {
            idt_regintr(_idt, vec, int_hdlr_table[vec], 0x8E, 2);
        }
    }

    _idtr.limit = sizeof(_idt) - 1;
    _idtr.base = (u64)&_idt;

    asm volatile("lidt %0" : : "m"(_idtr));
}