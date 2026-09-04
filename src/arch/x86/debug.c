#include <core/debug.h>
#include <core/mem/vmm.h>
#include <core/kprint.h>

struct kern_symbol* locate_symbol(u64 rip) {
    struct kern_symbol* closest = NULL;
    for (usize i = 0; i < nksyms; i++) {
        if (ksymtbl[i].addr <= rip) {
            closest = &ksymtbl[i];
        } else if (closest != NULL) {
            break;
        }
    }

    return closest;
}

void backtrace(u64 rbp) {
    if (!rbp) return;
    for (usize i = 0; i < 10; i++) {
        u64* fp = (u64*)rbp;
        if (!vmm_get_phys(vmm_cpml4v(), rbp)) break;
        rbp = fp[0];
        struct kern_symbol* sym = locate_symbol(fp[1]);
        const char* syms = (sym) ? sym->name : "unknown";
        kprint("%zu: %s (%zu)\n", i, syms, fp[1]);
        if (!rbp) break;
    }
}