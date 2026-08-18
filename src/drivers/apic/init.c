#include <core/std.h>
#include <drivers/pic.h>
#include <drivers/acpi.h>
#include <lai/core.h>
#include <drivers/term.h>
#include <core/asmh.h>

#define IA32_APIC_BASE_MSR 0x1B

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
    u8 smpid;
    u8 apicid;
    u32 flags;
} __attribute__((packed)) madt_plapic_t;

#define ENT_IOAPIC 0x01
typedef struct {
    u8 id;
    u8 __resv;
    u32 addr;
    u32 gsi_base;
} __attribute__((packed)) madt_ioapic_t;

#define ENT_IOAPIC_SRC_OVERRIDE 0x02
typedef struct {
    u8 bussrc;
    u8 irqsrc;
    u32 gsi;
    u16 flags;
} __attribute__((packed)) madt_ioaintso_t;

#define ENT_IOAPIC_NMI_SRC 0x03
typedef struct {
    u8 nmisrc;
    u8 __resv;
    u16 flags;
    u32 gsi;
} __attribute__((packed)) madt_ioanmisrc_t;

#define ENT_LOCALAPIC_NMI_INTS 0x04
typedef struct {
    u8 smpid;
    u16 flags;
    u8 lintno;
} __attribute__((packed)) madt_lanmiintrs_t;

#define ENT_LOCALAPIC_ADDR_OVERRIDE 0x05
typedef struct {
    u16 __resv;
    u64 addr;
} __attribute__((packed)) madt_laddro_t;

#define ENT_PROCLOCAL_X2APIC 0x09
typedef struct {
    u16 __resv;
    u32 x2apicid;
    u32 flags;
    u32 acpiid;
} __attribute__((packed)) madt_plx2apic_t;

int init_apic() {
    printf("[APIC] Disabling PIC\n");

    pic_disable();
    void* madt = NULL;
    if (!acpi_hdl->rsdt) {
        madt = find_acpitbl(acpi_hdl->xsdt, "APIC");
    } else {
        madt = find_acpitbl_32(acpi_hdl->rsdt, "APIC");
    }

    if (!madt) {
        printf("[APIC] Failed to find MADT\n");
        return -1;
    }

    printf("[APIC] Found MADT at address %p\n", madt);

    // bit 21 of ecx on leaf 0x01 will be 1 if x2APIC is supported
    u32 eax, ebx, ecx, edx;
    cpuid(0x01, 0, &eax, &ebx, &ecx, &edx);

    if (!(ecx & (1UL << 21))) {
        printf("[APIC] CPU does not support x2APIC which is required\n");
        return -1;
    }

    cpuid(0x0B, 0, &eax, &ebx, &ecx, &edx);
    u32 apicid = edx;

    madt_hdr_t* hdr = (madt_hdr_t*)madt;

    usize nents = 0;
    for (usize i = 0; i < hdr->hdr.len;) {
        nents++;
        madt_entry_hdr_t* enthdr = (madt_entry_hdr_t*)(madt + sizeof(*hdr) + i);
        i += enthdr->len;
    }

    // im too lazy to continue rn
    // so imma go write a gettimeofday syscall
}