#include <core/debug.h>

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