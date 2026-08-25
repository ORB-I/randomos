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

#define LAPIC_REG(offset) ((volatile uint32_t*)((uintptr_t)lapic_virt_addr + (offset)))
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
        *LAPIC_REG(0x310) = apicid << 24;
        *LAPIC_REG(0x300) = 0x00004500;

        do {
            asm volatile("pause" ::: "memory");
        } while (*LAPIC_REG(0x300) & (1 << 12));
        sleepms(10);

        for (int j = 0; j < 2; j++) {
            *LAPIC_REG(0x280) = 0;
            *LAPIC_REG(0x310) = apicid << 24;
            *LAPIC_REG(0x300) = 0x00046608;
            sleepms(1);

            do {
                asm volatile("pause" ::: "memory");
            } while (*LAPIC_REG(0x300) & (1 << 12));
        }
    }

    return numcores;
}

int smp_getactive() {
    int n = 0;
    for (usize i = 0; i < ncores; i++) {
        if (smp_info[i].acpi_ent->flags & 1) {
            serial_printf("CPU%d actie\n", i);
            n++;
        }
    }
    return n;
}