#include <drivers/rng/rng.h>
#include <drivers/rng/virtio_rng.h>
#include <core/std.h>
#include <core/asmh.h>
#include <core/udevr.h>
#include <core/kqueue.h>
#include <core/kprint.h>
#include <core/liballoc.h>

#define RNG_RDRAND 1
#define RNG_VIRTIO 2

static int rng_type = 0;
static kqueue_t* entq = NULL;

ssize rng_read(udev_t dev, void* buf, usize sz) {
    (void)dev;
    random_bytes(buf, sz);
    return sz;
}

int rng_init() {
    u32 a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    if ((int)((c >> 30) & 1)) {
        rng_type = RNG_RDRAND;
    } else if (virtio_rng_available()) {
        rng_type = RNG_VIRTIO;
    } else {
        return -1;
    }

    entq = kqueue_init(1024 * sizeof(u64));
    if (!entq) return -1;

    if (udevr_register(UDEV_RNG, "rng", UDEV_RD, rng_read, NULL) < 0) {
        kprint("Failed to register RNG with UDEVR\n");
        return 0;
    }

    udevr_regdev(UDEV_RNG, 0);

    return 0;
}

static int rng_fillpool() {
    if (kqueue_queued(entq) < sizeof(u64) * 32) {
        usize n = 1024 - (kqueue_queued(entq) / sizeof(u64));
        u64* tbuf = malloc(n * sizeof(u64));
        if (!tbuf) return -1;
        usize got = 0;

        if (rng_type == RNG_RDRAND) {
            for (got = 0; got < n; got++) {
                u8 ok;
                for (int j = 0; j < 10; j++) {
                    u64 buf;
                    asm volatile("rdrand %0\n\tsetc %1" : "=r"(buf), "=qm"(ok) :: "cc");
                    if (ok) {
                        tbuf[got] = buf;
                        break;
                    }
                }

                if (!ok) {
                    break;
                }
            }
        } else {
            got = virtio_rng_read((u8*)tbuf, n * sizeof(u64)) / sizeof(u64);
        }

        kqueue_enqueue(entq, (u8*)tbuf, got * sizeof(u64));
        free(tbuf);
        return 0;
    } else {
        return 0;
    }
}

u8 _randombyte() {
    if (rng_fillpool() < 0) return 0;
    u8 buf;
    kqueue_dequeue(entq, &buf, 1);
    return buf;
}

u64 random64() {
    if (rng_fillpool() < 0) return 0;
    u64 buf;
    usize got = kqueue_dequeue(entq, (u8*)&buf, sizeof(u64));
    if (got < sizeof(u64)) {
        return 0;
    }
    return buf;
}

int random_bytes(u8* buf, usize sz) {
    for (usize i = 0; i < sz; i++) {
        u8 rb = 0;
        while (rb == 0) {
            rb = _randombyte();
        }
        buf[i] = rb;
    }
    return 0;
}