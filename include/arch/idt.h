#pragma once
#include <core/std.h>

typedef struct {
    u16 isr_low;
    u16 kernel_cs;
    u8  ist;
    u8  attributes;
    u16 isr_mid;
    u32 isr_high;
    u32 reserved;
} __packed idt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} __packed idtr_t;

#define IDT_SIZE 256