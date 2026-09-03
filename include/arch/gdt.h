#pragma once
#include <core/std.h>

#define NULLSS 0
#define KCSS 1
#define KDSS 2
#define UCSS 3
#define UDSS 4
#define TSS 5

struct gdtr {
    u16 limit;
    u64 base;
} __packed;

struct gdt_entry {
    u16 limlow;
    u16 base_low;
    u8 base_mid;
    u8 acc;
    u8 gran;
    u8 base_high;
} __packed;

struct gdt_tss_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid1;
    u8  access;
    u8  granularity;
    u8  base_mid2;
    u32 base_high;
    u32 reserved;
} __packed;

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
} __packed;

void set_gdt_tss(struct gdt_entry* gdt, int n, u64 base, u32 lim, u8 acc);
void set_gdtent(struct gdt_entry* gdt, int n, u32 base, u32 lim, u8 acc, u8 gran);