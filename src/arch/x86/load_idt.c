#include <core/std.h>

#include <drivers/term.h>

typedef struct {
    u16 isr_low;
    u16 kernel_cs;
    u8  ist;
    u8  attributes;
    u16 isr_mid;
    u32 isr_high;
    u32 reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idtr_t;

#define IDT_SIZE 256
static idt_entry_t idt[IDT_SIZE];
static idtr_t idtr;

void idt_regintr(u8 vector, void* isr, u8 flags) {
    idt_entry_t* dsc = &idt[vector];
    u64 addr = (u64)isr;

    dsc->isr_low = addr & 0xFFFF;
    dsc->kernel_cs = 0x08;
    dsc->ist = 0;
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
            idt_regintr(vec, int_hdlr_table[vec], 0x8E);
        }
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;

    asm volatile("lidt %0" : : "m"(idtr));
}