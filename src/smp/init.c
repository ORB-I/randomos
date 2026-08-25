#include <drivers/apic.h>
#include <drivers/acpi.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <core/panic.h>
#include <drivers/time/clock.h>
#include <core/liballoc.h>
#include <core/printf.h>

extern u8 __smp_startup_begin[];
extern u8 __smp_startup_end[];
extern u8 __smp_startup_lma[];

typedef struct {
    u64 acpiid;
    u8 stack[16384];
} __attribute__((packed)) smp_stack_t;
smp_stack_t* smp_stacks = NULL;

typedef struct {
    u64 acpiid;
    u8 tid;
    madt_plapic_t* acpi_ent;
} smp_info_t;
smp_info_t* smp_info = NULL;

extern uintptr_t lapic_phys_addr;
extern volatile u32* lapic_virt_addr;
extern u32 bsp_lapic_id;

static usize ncores = 0;

extern u32 __smp_startup_cr3;
extern u64 __smp_stacks_lst;
extern u64 __smp_stacks_lstn;
static int init_smpcode(usize ncores) {
    smp_stacks = malloc(sizeof(smp_stack_t) * ncores);
    if (!smp_stacks) {
        panic("Failed to allocate SMP stacks");
    }

    usize codesz = (usize)__smp_startup_end - (usize)__smp_startup_begin;
    u64 cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    uintptr_t off_cr3 = (uintptr_t)&__smp_startup_cr3 - (uintptr_t)__smp_startup_begin;
    uintptr_t off_stk = (uintptr_t)&__smp_stacks_lst - (uintptr_t)__smp_startup_begin;
    uintptr_t off_stkn = (uintptr_t)&__smp_stacks_lstn - (uintptr_t)__smp_startup_begin;

    void* tgtv = (void*)(HHDM_START + 0x8000);
    memcpy(tgtv, (void*)__smp_startup_lma, codesz);

    volatile uint32_t* dcr3 = (volatile uint32_t*)(HHDM_START + 0x8000 + off_cr3);
    volatile uint64_t* dstk = (volatile uint64_t*)(HHDM_START + 0x8000 + off_stk);
    volatile uint64_t* dstkn = (volatile uint64_t*)(HHDM_START + 0x8000 + off_stkn);

    *dcr3 = (u32)cr3;
    *dstk = (u64)smp_stacks;
    *dstkn = (u64)ncores;

    return 0;
}

#define IPI_TRIGGER_EDGE 0x0
#define IPI_TRIGGER_LVL  0x1

#define IPI_LEVEL_DEASSERT 0x0
#define IPI_LEVEL_ASSERT   0x1

#define IPI_DSTMODE_PHYS 0x0
#define IPI_DSTMODE_LOG  0x1

#define IPI_DELMODE_FIXED 0x00
#define IPI_DELMODE_LOWP  0x01
#define IPI_DELMODE_SMI   0x02
#define IPI_DELMODE_RESV1 0x03
#define IPI_DELMODE_NMI   0x04
#define IPI_DELMODE_INIT  0x05
#define IPI_DELMODE_STUP  0x06
#define IPI_DELMODE_RESV2 0x07

#define LAPIC_REG(offset) ((volatile uint32_t*)((uintptr_t)lapic_virt_addr + (offset)))
void ipi_send(u8 dest, u8 trigger, u8 level, u8 dstmode, u8 delmode, u8 vec) {
    *LAPIC_REG(0x310) = dest << 24;
    u32 icr = ((u32)vec << 0) |
              ((u32)delmode << 8) |
              ((u32)dstmode << 11) |
              ((u32)level << 14) |
              ((u32)trigger << 15);
    *LAPIC_REG(0x300) = icr;
    do {
        asm volatile("pause" ::: "memory");
    } while (*LAPIC_REG(0x300) & (1 << 12));
}

int init_cores() {
    void* madt = NULL;
    if (acpi_hdl && acpi_hdl->xsdt) {
        madt = find_acpitbl(acpi_hdl->xsdt, "APIC");
    } else if (acpi_hdl && acpi_hdl->rsdt) {
        madt = find_acpitbl_32(acpi_hdl->rsdt, "APIC");
    }

    if (!madt) {
        panic("APIC: MADT table not found");
    }

    madt_hdr_t* hdr = (madt_hdr_t*)madt;

    u64 bptr = (u64)madt + sizeof(madt_hdr_t);
    u64 end = (u64)madt + hdr->hdr.len;

    u64 ptr = bptr;
    u64 numcores = 0;
    while (ptr < end) {
        madt_entry_hdr_t* ent = (madt_entry_hdr_t*)ptr;
        if (ent->len == 0) break;
        if (ent->type == ENT_PROCLOCAL_APIC) numcores++;
        ptr += ent->len;
    }

    init_smpcode(numcores);
    ncores = numcores;

    smp_info = malloc(sizeof(smp_info_t) * ncores);
    usize i = 0;
    ptr = bptr;
    while (ptr < end) {
        madt_entry_hdr_t* ent = (madt_entry_hdr_t*)ptr;
        if (ent->len == 0) break;
        if (ent->type == ENT_PROCLOCAL_APIC) smp_info[i++] = (smp_info_t){
            ((madt_plapic_t*)ptr)->apicid,
            i,
            (madt_plapic_t*)ptr
        };
        ptr += ent->len;
    }

    for (usize i = 0; i < numcores; i++) {
        u32 apicid = smp_info[i].acpiid;
        if (apicid == bsp_lapic_id) continue;
        ipi_send(apicid, IPI_TRIGGER_LVL, IPI_LEVEL_ASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_INIT, 0);
        sleepms(10);
        ipi_send(apicid, IPI_TRIGGER_LVL, IPI_LEVEL_DEASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_INIT, 0);
        sleepms(1);

        for (int j = 0; j < 2; j++) {
            *LAPIC_REG(0x280) = 0;
            ipi_send(apicid, IPI_TRIGGER_EDGE, 0, IPI_DSTMODE_PHYS, IPI_DELMODE_STUP, 0x80);
            sleepms(1);
        }
    }

    return numcores;
}

int smp_getactive() {
    int n = 0;
    for (usize i = 0; i < ncores; i++) {
        if (smp_info[i].acpi_ent->flags & 1) {
            serial_printf("CPU%d active\n", i);
            n++;
        }
    }
    return n;
}