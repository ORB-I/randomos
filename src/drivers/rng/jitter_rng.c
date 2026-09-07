#include <drivers/rng/jitter_rng.h>
#include <core/asmh.h>
#include <core/kprint.h>

bool jitter_rng_available(void) {
    return true;
}

int jitter_rng_init(void) {
    kprint("[RNG] Hardware CPU Jitter TRNG initialized\n");
    return 0;
}

int jitter_rng_read(u8* buf, usize len) {
    static u64 state = 0x9e3779b97f4a7c15ULL;

    for (usize i = 0; i < len; i++) {
        u64 t1 = rdtsc();
        u64 t2 = rdtsc();
        u64 delta = t2 - t1;

        state = state * 6364136223846793005ULL + delta + t2;
        u64 x = state ^ (state >> 30);
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= (x >> 27);
        buf[i] = (u8)x;
    }

    return (int)len;
}
