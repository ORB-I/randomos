#include <drivers/rng/intel_rng.h>
#include <core/mem/vmm.h>
#include <core/kprint.h>
#include <drivers/pci.h>

#define INTEL_FWH_RNG_PHYS_BASE 0xFFBC0000ULL
#define INTEL_FWH_RNG_STATUS    0x15F
#define INTEL_FWH_RNG_DATA      0x160

static volatile u8* intel_rng_base = NULL;

bool intel_rng_available(void) {
    bool intel_pci_present = false;
    for (u32 bus = 0; bus < 2; bus++) {
        for (u32 slot = 0; slot < 32; slot++) {
            pci_chdr_t hdr;
            pci_get_chdr(bus, slot, &hdr);
            if (hdr.vndid == 0x8086) {
                intel_pci_present = true;
                break;
            }
        }
        if (intel_pci_present) break;
    }

    if (!intel_pci_present) return false;

    volatile u8* status_reg = (volatile u8*)(HHDM_START + INTEL_FWH_RNG_PHYS_BASE + INTEL_FWH_RNG_STATUS);
    u8 status = *status_reg;
    if (status == 0xFF) return false;

    *status_reg = 0x01;
    status = *status_reg;
    return (status & 0x01) != 0;
}


int intel_rng_init(void) {
    if (!intel_rng_available()) {
        return -1;
    }
    intel_rng_base = (volatile u8*)(HHDM_START + INTEL_FWH_RNG_PHYS_BASE);
    kprint("[RNG] Intel 82802 / ICH Firmware Hub TRNG initialized\n");
    return 0;
}

int intel_rng_read(u8* buf, usize len) {
    if (!intel_rng_base) return 0;

    volatile u8* status_reg = intel_rng_base + INTEL_FWH_RNG_STATUS;
    volatile u8* data_reg = intel_rng_base + INTEL_FWH_RNG_DATA;

    usize got = 0;
    while (got < len) {
        bool ready = false;
        for (int retry = 0; retry < 100; retry++) {
            if (*status_reg & 0x01) {
                ready = true;
                break;
            }
            asm volatile("pause");
        }
        if (!ready) break;

        buf[got++] = *data_reg;
    }

    return (int)got;
}
