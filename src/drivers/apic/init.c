#include <core/std.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <core/panic.h>
#include <drivers/acpi.h>
#include <drivers/apic.h>
#include <drivers/pic.h>
#include <lai/core.h>
#include <core/printf.h>
#include <core/asmh.h>

uintptr_t lapic_phys_addr = 0xFEE00000;
volatile u32* lapic_virt_addr = NULL;
u32 bsp_lapic_id = 0;

static ioapic_info_t ioapics[MAX_IOAPICS];
static usize num_ioapics = 0;

static madt_ioaintso_t isos[MAX_ISOS];
static usize num_isos = 0;

static inline u32 lapic_read(u32 reg) {
    return *(volatile u32*)((uintptr_t)lapic_virt_addr + reg);
}

static inline void lapic_write(u32 reg, u32 val) {
    *(volatile u32*)((uintptr_t)lapic_virt_addr + reg) = val;
}

void lapic_eoi() {
    if (lapic_virt_addr) {
        lapic_write(LAPIC_EOI_REG, 0);
    }
}

static inline u32 ioapic_read(ioapic_info_t* ioapic, u32 reg) {
    volatile u32* regsel = (volatile u32*)((uintptr_t)ioapic->virt_addr + IOAPIC_REGSEL);
    volatile u32* iowin = (volatile u32*)((uintptr_t)ioapic->virt_addr + IOAPIC_IOWIN);
    *regsel = reg;
    return *iowin;
}

static inline void ioapic_write(ioapic_info_t* ioapic, u32 reg, u32 val) {
    volatile u32* regsel = (volatile u32*)((uintptr_t)ioapic->virt_addr + IOAPIC_REGSEL);
    volatile u32* iowin = (volatile u32*)((uintptr_t)ioapic->virt_addr + IOAPIC_IOWIN);
    *regsel = reg;
    *iowin = val;
}

static inline u64 ioapic_read64(ioapic_info_t* ioapic, u32 reg) {
    u32 low = ioapic_read(ioapic, reg);
    u32 high = ioapic_read(ioapic, reg + 1);
    return ((u64)high << 32) | low;
}

static inline void ioapic_write64(ioapic_info_t* ioapic, u32 reg, u64 val) {
    u32 low = (u32)(val & 0xFFFFFFFF);
    u32 high = (u32)(val >> 32);

    ioapic_write(ioapic, reg + 1, high);
    asm volatile("" ::: "memory");
    ioapic_write(ioapic, reg, low);
}

static ioapic_info_t* ioapic_for_gsi(u32 gsi, u32* offset) {
    for (usize i = 0; i < num_ioapics; i++) {
        if (gsi >= ioapics[i].gsi_base && gsi < (ioapics[i].gsi_base + ioapics[i].max_redirection_entries)) {
            *offset = gsi - ioapics[i].gsi_base;
            return &ioapics[i];
        }
    }
    return NULL;
}

void ioapic_route_gsi(u32 gsi, u8 vector, u32 lapic_id, u16 flags, bool masked) {
    u32 offset = 0;
    ioapic_info_t* ioapic = ioapic_for_gsi(gsi, &offset);
    if (!ioapic) {
        return;
    }

    u64 redirection = vector;

    if (flags & 2) {
        redirection |= (1UL << 13);
    }
    if (flags & 8) {
        redirection |= (1UL << 15);
    }
    if (masked) {
        redirection |= (1LL << 16);
    }

    redirection |= ((u64)lapic_id) << 56;

    u32 reg = IOAPIC_REDTBL(offset);
    ioapic_write64(ioapic, reg, redirection);
}

static u32 irq_to_gsi(u8 irq, u16* flags) {
    for (usize i = 0; i < num_isos; i++) {
        if (isos[i].irqsrc == irq) {
            if (flags) {
                *flags = isos[i].flags;
            }
            return isos[i].gsi;
        }
    }
    if (flags) {
        *flags = 0;
    }
    return (u32)irq;
}

void ioapic_set_irq(u8 irq, u8 vector, u32 lapic_id, bool masked) {
    u16 flags = 0;
    u32 gsi = irq_to_gsi(irq, &flags);
    ioapic_route_gsi(gsi, vector, lapic_id, flags, masked);
}

u32 get_lapic_id() {
    return bsp_lapic_id;
}

void ioapic_mask_irq(u8 irq) {
    u16 flags = 0;
    u32 gsi = irq_to_gsi(irq, &flags);
    u32 offset = 0;
    ioapic_info_t* ioapic = ioapic_for_gsi(gsi, &offset);
    if (!ioapic) {
        return;
    }
    u32 reg = IOAPIC_REDTBL(offset);
    u32 low = ioapic_read(ioapic, reg);
    low |= (1 << 16);
    ioapic_write(ioapic, reg, low);
}

void ioapic_unmask_irq(u8 irq) {
    u16 flags = 0;
    u32 gsi = irq_to_gsi(irq, &flags);
    u32 offset = 0;
    ioapic_info_t* ioapic = ioapic_for_gsi(gsi, &offset);
    if (!ioapic) {
        return;
    }
    u32 reg = IOAPIC_REDTBL(offset);
    u32 low = ioapic_read(ioapic, reg);
    low &= ~(1 << 16);
    ioapic_write(ioapic, reg, low);
}

static void parse_madt(void* madt) {
    madt_hdr_t* hdr = (madt_hdr_t*)madt;
    lapic_phys_addr = hdr->lapic_addr;

    uintptr_t ptr = (uintptr_t)madt + sizeof(madt_hdr_t);
    uintptr_t end = (uintptr_t)madt + hdr->hdr.len;

    asm volatile(
        "mov $1, %%eax\n\t"
        "cpuid\n\t"
        "shrl $24, %%ebx\n\t"
        : "=b"(bsp_lapic_id) :: 
    );
    
    while (ptr < end) {
        madt_entry_hdr_t* entry = (madt_entry_hdr_t*)ptr;
        if (entry->len == 0) {
            break;
        }

        switch (entry->type) {
            case ENT_IOAPIC: {
                if (num_ioapics < MAX_IOAPICS) {
                    madt_ioapic_t* ioapic = (madt_ioapic_t*)ptr;
                    ioapics[num_ioapics].id = ioapic->id;
                    ioapics[num_ioapics].gsi_base = ioapic->gsi_base;
                    ioapics[num_ioapics].phys_addr = ioapic->addr;
                    num_ioapics++;
                }
                break;
            }
            case ENT_IOAPIC_SRC_OVERRIDE: {
                if (num_isos < MAX_ISOS) {
                    madt_ioaintso_t* iso = (madt_ioaintso_t*)ptr;
                    isos[num_isos] = *iso;
                    num_isos++;
                }
                break;
            }
            case ENT_LOCALAPIC_ADDR_OVERRIDE: {
                madt_laddro_t* laddro = (madt_laddro_t*)ptr;
                lapic_phys_addr = laddro->addr;
                break;
            }
            default:
                break;
        }
        ptr += entry->len;
    }
}

/* runs on whichever core calls it, an AP must call this itself because
   INIT leaves its lapic software-disabled so it can neither send nor
   receive IPIs until the SVR enable bit is set */
void apic_enable_current() {
    u64 apic_base = rdmsr(IA32_APIC_BASE_MSR);
    apic_base |= IA32_APIC_BASE_MSR_ENABLE;
    wrmsr(IA32_APIC_BASE_MSR, apic_base);

    lapic_write(LAPIC_TPR_REG, 0);
    lapic_write(LAPIC_SVR_REG, 0x100 | LAPIC_SPURIOUS_VEC);
    lapic_write(LAPIC_LVT_TIMER, 1 << 16);
    lapic_write(LAPIC_LVT_LINT0, 1 << 16);
    lapic_write(LAPIC_LVT_LINT1, 1 << 16);
    lapic_write(LAPIC_LVT_ERROR, 1 << 16);
}

static void enable_lapic() {
    apic_enable_current();

    bsp_lapic_id = (lapic_read(LAPIC_ID_REG) >> 24) & 0xFF;
    printf("LAPIC ID %d\n", bsp_lapic_id);
}

void apic_init() {
    pic_disable();

    void* madt = NULL;
    if (acpi_hdl && acpi_hdl->xsdt) {
        madt = find_acpitbl(acpi_hdl->xsdt, "APIC");
    } else if (acpi_hdl && acpi_hdl->rsdt) {
        madt = find_acpitbl_32(acpi_hdl->rsdt, "APIC");
    }

    if (!madt) {
        panic("APIC: MADT table not found");
    }

    parse_madt(madt);

    lapic_virt_addr = (volatile u32*)(lapic_phys_addr + HHDM_START);
    enable_lapic();

    for (usize i = 0; i < num_ioapics; i++) {
        ioapics[i].virt_addr = (volatile u32*)(ioapics[i].phys_addr + HHDM_START);
        u32 ver = ioapic_read(&ioapics[i], IOAPIC_VER);
        ioapics[i].max_redirection_entries = ((ver >> 16) & 0xFF) + 1;

        for (u32 j = 0; j < ioapics[i].max_redirection_entries; j++) {
            u32 reg = IOAPIC_REDTBL(j);
            ioapic_write(&ioapics[i], reg, 0x00010000 | (0x20 + j));
            ioapic_write(&ioapics[i], reg + 1, ((u32)bsp_lapic_id) << 24);
        }
    }
}