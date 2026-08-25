#pragma once

#include <core/std.h>
#include <drivers/acpi.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#define LAPIC_ID_REG 0x0020
#define LAPIC_VER_REG 0x0030
#define LAPIC_TPR_REG 0x0080
#define LAPIC_EOI_REG 0x00B0
#define LAPIC_SVR_REG 0x00F0
#define LAPIC_ESR_REG 0x0280
#define LAPIC_LVT_TIMER 0x0320
#define LAPIC_LVT_LINT0 0x0350
#define LAPIC_LVT_LINT1 0x0360
#define LAPIC_LVT_ERROR 0x0370

#define IOAPIC_REGSEL 0x00
#define IOAPIC_IOWIN 0x10

#define IOAPIC_ID 0x00
#define IOAPIC_VER 0x01
#define IOAPIC_ARB 0x02
#define IOAPIC_REDTBL(n) (0x10 + 2 * (n))

#define MAX_IOAPICS 8
#define MAX_ISOS 32

typedef struct {
    sdt_header_t hdr;
    u32 lapic_addr;
    u32 flags;
} __attribute__((packed)) madt_hdr_t;

typedef struct {
    u8 type;
    u8 len;
} __attribute__((packed)) madt_entry_hdr_t;

#define ENT_PROCLOCAL_APIC 0x00
typedef struct {
    madt_entry_hdr_t hdr;
    u8 smpid;
    u8 apicid;
    u32 flags;
} __attribute__((packed)) madt_plapic_t;

#define ENT_IOAPIC 0x01
typedef struct {
    madt_entry_hdr_t hdr;
    u8 id;
    u8 __resv;
    u32 addr;
    u32 gsi_base;
} __attribute__((packed)) madt_ioapic_t;

#define ENT_IOAPIC_SRC_OVERRIDE 0x02
typedef struct {
    madt_entry_hdr_t hdr;
    u8 bussrc;
    u8 irqsrc;
    u32 gsi;
    u16 flags;
} __attribute__((packed)) madt_ioaintso_t;

#define ENT_LOCALAPIC_ADDR_OVERRIDE 0x05
typedef struct {
    madt_entry_hdr_t hdr;
    u16 __resv;
    u64 addr;
} __attribute__((packed)) madt_laddro_t;

typedef struct {
    madt_entry_hdr_t hdr;
    u8 id;
    u32 gsi_base;
    u32 max_redirection_entries;
    uintptr_t phys_addr;
    volatile u32* virt_addr;
} ioapic_info_t;

void apic_init();
void lapic_eoi();
void ioapic_set_irq(u8 irq, u8 vector, u32 lapic_id, bool masked);
void ioapic_mask_irq(u8 irq);
void ioapic_unmask_irq(u8 irq);
void ioapic_route_gsi(u32 gsi, u8 vector, u32 lapic_id, u16 flags, bool masked);
u32 get_lapic_id();
u8 get_apicid();
