#include <drivers/rng/rng.h>
#include <core/std.h>
#include <core/asmh.h>

u64 random64() {
    int stat;
    u64 buf;
    asm volatile(
        "rdrand %0\n\t"
        "setc %1"
        : "=r"(buf), "=qm"(stat)
    );
    if (stat) return buf;
    else return 0;
}

int random_bytes(u8* buf, usize sz) {
    for (usize i = 0; i < sz; i++) {
        u64 rb = 0;
        while (rb == 0) {
            rb = random64();
        }
        buf[i] = rb & 0xFF;
    }
    return 0;
}