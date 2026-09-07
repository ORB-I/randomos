#include <drivers/rng/via_rng.h>
#include <core/kprint.h>
#include <lib/string.h>

bool via_rng_available(void) {
    u32 max_ext, b, c, d;
    asm volatile("cpuid" : "=a"(max_ext), "=b"(b), "=c"(c), "=d"(d) : "a"(0xC0000000));
    if (max_ext < 0xC0000001) return false;

    u32 a_out, b_out, c_out, d_out;
    asm volatile("cpuid" : "=a"(a_out), "=b"(b_out), "=c"(c_out), "=d"(d_out) : "a"(0xC0000001));
    // Bit 2: RNG present, Bit 3: RNG enabled
    return (d_out & (1 << 2)) && (d_out & (1 << 3));
}

int via_rng_init(void) {
    if (!via_rng_available()) {
        return -1;
    }
    kprint("[RNG] VIA PadLock Quantum TRNG detected and initialized\n");
    return 0;
}

int via_rng_read(u8* buf, usize len) {
    usize offset = 0;
    while (offset < len) {
        u8 temp[16] __attribute__((aligned(16)));
        u32 count = 0;
        u32 edx_val = 0;

        // rep xstore-rng
        asm volatile(

            ".byte 0x0f, 0xa7, 0xc0"
            : "=a"(count)
            : "D"(temp), "d"(edx_val)
            : "memory"
        );

        u32 bytes_got = count & 0x1F;
        if (bytes_got == 0 || bytes_got > sizeof(temp)) {
            break;
        }

        usize to_copy = len - offset;
        if (to_copy > bytes_got) to_copy = bytes_got;
        memcpy(buf + offset, temp, to_copy);
        offset += to_copy;
    }

    return (int)offset;
}
