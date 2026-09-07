#include <drivers/rng/jitter_rng.h>
#include <core/asmh.h>
#include <core/kprint.h>

static volatile u64 jitter_mem[64];

static u64 sample_jitter(void) {
    u64 t1 = rdtsc();
    for (int i = 0; i < 16; i++) {
        jitter_mem[i] ^= (t1 + i);
    }
    u64 t2 = rdtsc();
    return t2 - t1;
}

bool jitter_rng_available(void) {
    return true;
}

int jitter_rng_init(void) {
    kprint("[RNG] Hardware CPU Jitter TRNG initialized\n");
    return 0;
}

static int harvest_bit(void) {
    for (int retry = 0; retry < 1000; retry++) {
        u64 delta1 = sample_jitter();
        u64 delta2 = sample_jitter();

        int bit1 = (delta1 ^ (delta1 >> 1) ^ (delta1 >> 2)) & 1;
        int bit2 = (delta2 ^ (delta2 >> 1) ^ (delta2 >> 2)) & 1;

        if (bit1 == 0 && bit2 == 1) return 0;
        if (bit1 == 1 && bit2 == 0) return 1;
    }
    return -1;
}

int jitter_rng_read(u8* buf, usize len) {
    for (usize byte_idx = 0; byte_idx < len; byte_idx++) {
        u8 val = 0;
        for (int bit = 0; bit < 8; bit++) {
            int b = harvest_bit();
            if (b < 0) {
                u64 t = sample_jitter();
                b = (t ^ (t >> 3) ^ (t >> 7)) & 1;
            }

            val = (val << 1) | (b & 1);
        }
        buf[byte_idx] = val;
    }

    return (int)len;
}
