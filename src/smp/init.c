#include <drivers/apic.h>
#include <drivers/acpi.h>
#include <core/mem/vmm.h>
#include <lib/string.h>
#include <core/panic.h>
#include <drivers/time/clock.h>
#include <core/liballoc.h>
#include <core/printf.h>
#include <core/idt.h>
#include <smp/ipi.h>
#include <smp/smp.h>
#include <smp/apreq.h>

extern u8 __smp_startup_begin[];
extern u8 __smp_startup_end[];
extern u8 __smp_startup_lma[];

smp_stack_t* smp_stacks = NULL;

smp_info_t* smp_info = NULL;
ap_req_t* apreqvec = NULL;
ap_state* apstates = NULL;

extern u32 bsp_lapic_id;

usize ncores = 0;

extern u32 __smp_startup_cr3;
extern u64 __smp_stacks_lst;
extern u64 __smp_stacks_lstn;
static int init_smpcode(usize ncores) {
    /* trampoline page is reserved in pmm_init so this copy stays put */
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

void ipi_send(u8 dest, u8 shrtdst, u8 trigger, u8 level, u8 dstmode, u8 delmode, u8 vec) {
    *LAPIC_REG(0x310) = dest << 24;
    u32 icr = ((u32)vec << 0) |
              ((u32)delmode << 8) |
              ((u32)dstmode << 11) |
              ((u32)level << 14) |
              ((u32)trigger << 15) |
              ((u32)shrtdst << 18);

    *LAPIC_REG(0x300) = icr;
    do {
        asm volatile("pause" ::: "memory");
    } while (*LAPIC_REG(0x300) & (1 << 12));
}

extern void smp_request_hdlr();
extern void bsp_request_hdlr();
extern void ap_stop_hdlr();
extern void lapic_spurious_hdlr();

thread_idt_t* tidts = NULL;
thread_gdt_t* tgdts = NULL;
extern void (*int_hdlr_table[])();

void init_smpreqs() {
    idt_regintr(NULL, 255, bsp_request_hdlr, 0x8E, 1);
    idt_regintr(NULL, LAPIC_SPURIOUS_VEC, lapic_spurious_hdlr, 0x8E, 0);
    bsp_apicid = bsp_lapic_id;

    apreqvec = malloc(sizeof(ap_req_t) * ncores);
    tidts = malloc(sizeof(thread_idt_t) * ncores);
    tgdts = malloc(sizeof(thread_gdt_t) * ncores);
    apstates = malloc(sizeof(ap_state) * ncores);

    if (!apreqvec || !tidts || !tgdts || !apstates) {
        panic("Failed to initialize SMPs");
    }

    for (usize i = 0; i < ncores; i++) {
        thread_gdt_t* tgdt = &tgdts[i];
        thread_idt_t* tidt = &tidts[i];

        tgdt->apicid = smp_info[i].apicid;
        set_gdtent(tgdt->gdt, NULLSS, 0, 0, 0, 0);
        set_gdtent(tgdt->gdt, KCSS, 0, 0xFFFFFFFF, 0x9A, 0x20);
        set_gdtent(tgdt->gdt, KDSS, 0, 0xFFFFFFFF, 0x92, 0x00);
        set_gdtent(tgdt->gdt, UCSS, 0, 0xFFFFFFFF, 0xFA, 0x20);
        set_gdtent(tgdt->gdt, UDSS, 0, 0xFFFFFFFF, 0xF2, 0x00);

        memset(&tgdt->tss, 0, sizeof(tgdt->tss));

        tgdt->tss.rsp0 = (u64)(smp_stacks[i].stack + sizeof(smp_stacks[i].stack));
        tgdt->tss.iomap_base = sizeof(struct tss_entry);

        u64 tssb = (u64)&tgdt->tss;
        u64 tssl = sizeof(struct tss_entry) - 1;
        set_gdt_tss(tgdt->gdt, TSS, tssb, tssl, 0x89);

        tgdt->gdtr.limit = sizeof(tgdt->gdt) - 1;
        tgdt->gdtr.base = (u64)tgdt->gdt;

        s32 vectors[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19};
        s32 count = sizeof(vectors) / sizeof(vectors[0]);

        for (s32 i = 0; i < count; i++) {
            s32 vec = vectors[i];
            if (int_hdlr_table[vec]) {
                idt_regintr(tidt->idt, vec, int_hdlr_table[vec], 0x8E, 0);
            }
        }

        idt_regintr(tidt->idt, AP_MSGVEC, smp_request_hdlr, 0x8E, 0);
        idt_regintr(tidt->idt, AP_STOPVEC, ap_stop_hdlr, 0x8E, 0);
        idt_regintr(tidt->idt, LAPIC_SPURIOUS_VEC, lapic_spurious_hdlr, 0x8E, 0);

        tidt->idtr.limit = sizeof(tidt->idt) - 1;
        tidt->idtr.base = (u64)&tidt->idt;

        apreqvec[i] = (ap_req_t){0,0,smp_info[i].apicid,0,NULL,0};
    }
}

u64 bsp_apicid = 0;
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

    usize i = 0;
    ptr = bptr;
    smp_info = malloc(sizeof(smp_info_t) * ncores);
    if (!smp_info) {
        panic("failed to allocate smp info");
    }
    
    while (ptr < end) {
        madt_entry_hdr_t* ent = (madt_entry_hdr_t*)ptr;
        if (ent->len == 0) break;
        if (ent->type == ENT_PROCLOCAL_APIC) {
            smp_info[i] = (smp_info_t){
                ((madt_plapic_t*)ptr)->apicid,
                i,
                (madt_plapic_t*)ptr,
                SMP_STATUS_DEAD
            };
            i++;
        }
        ptr += ent->len;
    }
    init_smpreqs();

    /* the trampoline matches its cpuid APIC id against these apicid
       fields to locate its own stack, so they need real values before
       any SIPI goes out */
    for (usize i = 0; i < ncores; i++) {
        smp_stacks[i].apicid = smp_info[i].apicid;
    }

    for (usize i = 0; i < numcores; i++) {
        u32 apicid = smp_info[i].apicid;
        if (apicid == bsp_lapic_id) continue;
        serial_printf("Starting SMP %d\n", apicid);
        ipi_send(apicid, IPI_SHRTDST_NONE, IPI_TRIGGER_LVL, IPI_LEVEL_ASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_INIT, 0);
        sleepms(10);
        ipi_send(apicid, IPI_SHRTDST_NONE, IPI_TRIGGER_LVL, IPI_LEVEL_DEASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_INIT, 0);
        sleepms(1);

        for (int j = 0; j < 2; j++) {
            *LAPIC_REG(0x280) = 0;
            ipi_send(apicid, IPI_SHRTDST_NONE, IPI_TRIGGER_EDGE, IPI_LEVEL_ASSERT, IPI_DSTMODE_PHYS, IPI_DELMODE_STUP, 0x08);
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