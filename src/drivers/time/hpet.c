#include <drivers/acpi.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <drivers/apic.h>
#include <core/printf.h>
#include <core/idt.h>
#include <drivers/time/hpet.h>

typedef struct {
    sdt_header_t hdr;
    u32 evttblkid; // event timer block id
    genaddr_t baseaddr;
    u8 hpetno;
    u16 mcmctpm; // main count clock tick in periodic mode
    u8 attrs;
} __attribute__((packed)) hpet_acpitbl_t;

hpet_acpitbl_t* hpet_acpitbl = NULL;

u8 hpet_read8(usize reg) {
    return *((volatile u8*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg));
}

u16 hpet_read16(usize reg) {
    return *((volatile u16*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg));
}

u32 hpet_read32(usize reg) {
    return *((volatile u32*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg));
}

u64 hpet_read64(usize reg) {
    return *((volatile u64*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg));
}

void hpet_write8(usize reg, u8 val) {
    *((volatile u8*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg)) = val;
}

void hpet_write16(usize reg, u16 val) {
    *((volatile u16*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg)) = val;
}

void hpet_write32(usize reg, u32 val) {
    *((volatile u32*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg)) = val;
}

void hpet_write64(usize reg, u64 val) {
    *((volatile u64*)(HHDM_START + hpet_acpitbl->baseaddr.addr + reg)) = val;
}

extern void hpet_hdlr();
u64 _hpet_tickcnt = 0;

u64 hpet_getms() {
    return _hpet_tickcnt;
}

int hpet_init(u64 (**getms)(void)) {
    void* acpitbl_ptr = NULL;
    if (acpi_hdl->xsdt) {
        acpitbl_ptr = find_acpitbl(acpi_hdl->xsdt, "HPET");
    } else {
        acpitbl_ptr = find_acpitbl_32(acpi_hdl->rsdt, "HPET");
    }

    if (!acpitbl_ptr) {
        return -1;
    }

    hpet_acpitbl = (hpet_acpitbl_t*)acpitbl_ptr;
    u64 capid = hpet_read64(0x00);
    if (!(capid & (1 << 13))) {
        hpet_acpitbl = NULL;
        return -1;
    }

    u8 numtims = (capid >> 8) & 0x1F;
    if (numtims < 2) {
        return -1; // the whole point
                   // of this driver outside of a clock source
                   // is for a preemptive timer so we'll need 2
    }
    u32 clkperiod = capid >> 32;

    u64 genconf = hpet_read64(0x10);
    genconf &= ~0x01;
    hpet_write64(0x10, genconf);

    for (u8 i = 2; i < numtims; i++) {
        hpet_write64(0x100 + (0x20 * i), 0);
    }

    u64 tm0cap = hpet_read64(0x100);
    if (!(tm0cap & (1 << 4))) {
        hpet_write64(0x100, 0);
        hpet_acpitbl = NULL;
        return -1;
    }

    u64 tm0cfg = tm0cap;
    tm0cfg &= ~(0x1F << 9);
    tm0cfg |= (20 << 9);
    tm0cfg |= (1 << 3);
    tm0cfg |= (1 << 2);
    tm0cfg |= (1 << 6);

    hpet_write64(0x100, tm0cfg);
    u64 counter = hpet_read64(0xF0);
    u64 period = 1000000000000ULL / clkperiod;
    serial_printf("HPET period: %u fs\n", clkperiod);
serial_printf("HPET ticks/ms: %llu\n", period);
    hpet_write64(0x108, counter + period);

    idt_regintr(0x40, hpet_hdlr, 0x8E, 1);
    ioapic_set_irq(20, 0x40, get_lapic_id(), 0);
    ioapic_unmask_irq(20);

    genconf = hpet_read64(0x10);
    genconf |= 0x01;
    hpet_write64(0x10, genconf);

    u64 tm1cap = hpet_read64(0x120);
    u64 tm1cfg = tm1cap;

    tm1cfg &= ~(0x1FULL << 9);
    tm1cfg &= ~(1ULL << 3);
    tm1cfg &= ~(1ULL << 6);
    tm1cfg &=  ~(1ULL << 2); // disable for now

    hpet_write64(0x120, tm1cfg);

    *getms = hpet_getms;
    return 0;
}

typedef struct {
    u64 time;
    u8 irq;
    u8 timerid;
} preemptive_timer_t;

int hpet_mkpreemptive_timer(preemptive_timer_t* buf, u64 ms, void(*hdlr)(void)) {
    idt_regintr(0x3F, hdlr, 0x8E, 1);
    ioapic_set_irq(19, 0x3F, get_lapic_id(), 0);
    u32 clkprd = hpet_read64(0x00) >> 32;

    buf->irq = 19;
    buf->timerid = 1;
    buf->time = (1000000000000ULL / (u64)clkprd) * ms;

    return 0;
}

int hpet_start_preemptive(preemptive_timer_t* timer) {
    // HPET periodic mode requires:
    //  1. set TN_TYPE_CNF (periodic, bit 3) + TN_INT_ENB (bit 2) +
    //     TN_VAL_SET_CNF (bit 6) in the timer config
    //  2. write the *period* (not an absolute target) to the comparator
    // VAL_SET tells the hardware the next comparator write is the reload
    // value; without it the timer fires once and stops.
    u64 cfg = hpet_read64(0x120);
    cfg &= ~(0x1FULL << 9);
    cfg |= ((u64)timer->irq << 9);
    cfg |= (1ULL << 3) | (1ULL << 2) | (1ULL << 6);
    hpet_write64(0x120, cfg);
    hpet_write64(0x128, timer->time);
    ioapic_unmask_irq(timer->irq);

    return 0;
}

int hpet_active() {
    return (hpet_acpitbl != NULL);
}

static int _hpet_pollcnt = 0;
int _hpet_pollrun = 0;
void krunpolls();

void c_hpet_hdlr() {
    _hpet_tickcnt++;
    _hpet_pollcnt++;
    if (_hpet_pollcnt >= 1000) {
        if (_hpet_pollrun) {
            _hpet_pollcnt = 0;
            page_table_t* pt = vmm_cpml4v();
            vmm_skasp();
            krunpolls();
            vmm_sasp(pt);
        }
        _hpet_pollcnt = 0;
    }
    lapic_eoi();
}
