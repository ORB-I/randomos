#include <drivers/rng/rdseed.h>
#include <core/kprint.h>
#include <lib/string.h>

bool rdseed_available(void) {
    u32 max_leaf, b, c, d;
    asm volatile("cpuid" : "=a"(max_leaf), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    if (max_leaf < 7) return false;

    u32 a_out, b_out, c_out, d_out;
    asm volatile("cpuid" : "=a"(a_out), "=b"(b_out), "=c"(c_out), "=d"(d_out) : "a"(7), "c"(0));
    return (b_out & (1 << 18)) != 0;
}

static inline bool rdseed64_step(u64* val) {
    u8 ok;
    asm volatile("rdseed %0; setc %1" : "=r"(*val), "=qm"(ok) :: "cc");
    return ok != 0;
}

int rdseed_init(void) {
    if (!rdseed_available()) {
        return -1;
    }
    kprint("[RNG] Intel/AMD RDSEED hardware RNG detected and initialized\n");
    return 0;
}

int rdseed_read(u8* buf, usize len) {
    usize offset = 0;
    while (offset < len) {
        u64 val = 0;
        bool success = false;
        for (int retry = 0; retry < 64; retry++) {
            if (rdseed64_step(&val)) {
                success = true;
                break;
            }
            asm volatile("pause");
        }
        if (!success) {
            break;
        }

        usize to_copy = len - offset;
        if (to_copy > sizeof(u64)) to_copy = sizeof(u64);
        memcpy(buf + offset, &val, to_copy);
        offset += to_copy;
    }

    return (int)offset;
}
