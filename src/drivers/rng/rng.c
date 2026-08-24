#include <drivers/rng/rng.h>
#include <core/std.h>
#include <core/asmh.h>

// rdrand is only executed after cpuid says it exists - qemu's default
// cpu (qemu64) does not expose it and blindly executing it used to #UD
// the whole kernel out of lwip's LWIP_RAND().
static int has_rdrand = -1;
static u64 prng_state = 0;

static void rng_probe() {
    u32 a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    has_rdrand = (int)((c >> 30) & 1);

    // seed the fallback from the TSC so two boots diverge
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    prng_state = ((u64)hi << 32) | lo;
    if (prng_state == 0) prng_state = 0x9E3779B97F4A7C15ULL;
}

static u64 prng_next() {
    // stir the TSC in so callers in a tight loop do not see a fixed
    // sequence; xorshift64* on top for diffusion
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    prng_state ^= ((u64)hi << 32) | lo;
    prng_state ^= prng_state >> 12;
    prng_state ^= prng_state << 25;
    prng_state ^= prng_state >> 27;
    return prng_state * 0x2545F4914F6CDD1DULL;
}

u64 random64() {
    if (has_rdrand < 0) {
        rng_probe();
    }

    if (has_rdrand) {
        // SDM says to check CF and retry; give up after a few tries
        for (int i = 0; i < 10; i++) {
            u64 buf;
            u8 ok;
            asm volatile("rdrand %0\n\tsetc %1" : "=r"(buf), "=qm"(ok) :: "cc");
            if (ok) return buf;
        }
        return 0;
    }

    return prng_next();
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
