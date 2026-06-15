#include <core/std.h>

#include <drivers/vga.h>

typedef struct {
    u16 isr_low;
    u16 kernel_cs;
    u8 reserved;
    u8 attributes;
    u16 isr_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idtr_t;

#define IDT_SIZE 256
static idt_entry_t idt[IDT_SIZE];
static idtr_t idtr;

void idt_regintr(u8 vector, void* isr, u8 flags) {
    idt_entry_t* dsc = &idt[vector];

    dsc->isr_low = (u32)isr & 0xFFFF;
    dsc->kernel_cs = 0x08;
    dsc->attributes = flags;
    dsc->isr_high = (u32)isr >> 16;
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
    idtr.base = (u32)&idt;

    asm volatile("lidt %0" : : "m"(idtr));
}