#include <drivers/rng/rng.h>
#include <drivers/rng/virtio_rng.h>
#include <drivers/rng/rdseed.h>
#include <drivers/rng/via_rng.h>
#include <drivers/rng/intel_rng.h>
#include <drivers/rng/tpm_rng.h>
#include <drivers/rng/jitter_rng.h>
#include <core/std.h>
#include <core/asmh.h>
#include <core/udevr.h>
#include <core/kqueue.h>
#include <core/kprint.h>
#include <core/liballoc.h>
#include <lib/string.h>

static kqueue_t* entq = NULL;

static bool rdrand_available(void) {
    u32 a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    return ((c >> 30) & 1) != 0;
}

static int rdrand_init(void) {
    kprint("[RNG] Intel/AMD RDRAND hardware RNG detected and initialized\n");
    return 0;
}

static int rdrand_read(u8* buf, usize len) {
    usize offset = 0;
    while (offset < len) {
        u64 val;
        u8 ok;
        bool success = false;
        for (int retry = 0; retry < 32; retry++) {
            asm volatile("rdrand %0\n\tsetc %1" : "=r"(val), "=qm"(ok) :: "cc");
            if (ok) {
                success = true;
                break;
            }
            asm volatile("pause");
        }
        if (!success) break;

        usize to_copy = len - offset;
        if (to_copy > sizeof(u64)) to_copy = sizeof(u64);
        memcpy(buf + offset, &val, to_copy);
        offset += to_copy;
    }
    return (int)offset;
}

static int virtio_driver_init(void) {
    kprint("[RNG] VirtIO hardware RNG detected and initialized\n");
    return 0;
}

static int virtio_driver_read(u8* buf, usize len) {
    return (int)virtio_rng_read(buf, len);
}

static hw_rng_driver_t rng_drivers[] = {
    {"RDSEED", rdseed_available, rdseed_init, rdseed_read, false},
    {"RDRAND", rdrand_available, rdrand_init, rdrand_read, false},
    {"VirtIO RNG", virtio_rng_available, virtio_driver_init, virtio_driver_read, false},
    {"VIA PadLock", via_rng_available, via_rng_init, via_rng_read, false},
    {"Intel 82802 FWH", intel_rng_available, intel_rng_init, intel_rng_read, false},
    {"TPM RNG", tpm_rng_available, tpm_rng_init, tpm_rng_read, false},
    {"CPU Jitter TRNG", jitter_rng_available, jitter_rng_init, jitter_rng_read, false},
};

#define NUM_RNG_DRIVERS (sizeof(rng_drivers) / sizeof(rng_drivers[0]))

ssize rng_read(udev_t dev, void* buf, usize sz) {
    (void)dev;
    if (random_bytes(buf, sz) < 0) return -1;
    return sz;
}

int rng_init(void) {
    entq = kqueue_init(1024 * sizeof(u64));
    if (!entq) return -1;

    int active_drivers = 0;
    for (usize i = 0; i < NUM_RNG_DRIVERS; i++) {
        if (rng_drivers[i].available && rng_drivers[i].available()) {
            if (rng_drivers[i].init && rng_drivers[i].init() == 0) {
                rng_drivers[i].active = true;
                active_drivers++;
            }
        }
    }

    if (active_drivers == 0) {
        kprint("[RNG] No hardware RNG sources available\n");
        return -1;
    }

    kprint("[RNG] Subsystem ready with %d active hardware entropy source(s)\n", active_drivers);

    if (udevr_register(UDEV_RNG, "rng", UDEV_RD, rng_read, NULL) < 0) {
        kprint("[RNG] Failed to register RNG with UDEVR\n");
        return 0;
    }

    udevr_regdev(UDEV_RNG, 0);
    return 0;
}

static int rng_fillpool(void) {
    if (kqueue_queued(entq) < sizeof(u64) * 32) {
        usize n_bytes = (1024 * sizeof(u64)) - kqueue_queued(entq);
        u8* tbuf = malloc(n_bytes);
        if (!tbuf) return -1;
        memset(tbuf, 0, n_bytes);

        u8* mixbuf = malloc(n_bytes);
        if (!mixbuf) {
            free(tbuf);
            return -1;
        }

        usize collected = 0;
        bool any_collected = false;

        for (usize i = 0; i < NUM_RNG_DRIVERS; i++) {
            if (rng_drivers[i].active) {
                int r = rng_drivers[i].read(mixbuf, n_bytes);
                if (r > 0) {
                    any_collected = true;
                    if ((usize)r > collected) collected = (usize)r;
                    for (usize j = 0; j < (usize)r; j++) {
                        tbuf[j] ^= mixbuf[j];
                    }
                }
            }
        }

        free(mixbuf);

        if (any_collected && collected > 0) {
            kqueue_enqueue(entq, tbuf, collected);
        }

        free(tbuf);
        return any_collected ? 0 : -1;
    }
    return 0;
}

u8 _randombyte(void) {
    u8 buf = 0;
    random_bytes(&buf, 1);
    return buf;
}

u64 random64(void) {
    u64 buf = 0;
    random_bytes((u8*)&buf, sizeof(u64));
    return buf;
}

int random_bytes(u8* buf, usize sz) {
    usize got = 0;
    while (got < sz) {
        if (rng_fillpool() < 0 && kqueue_queued(entq) == 0) {
            for (usize i = 0; i < NUM_RNG_DRIVERS; i++) {
                if (rng_drivers[i].active) {
                    int r = rng_drivers[i].read(buf + got, sz - got);
                    if (r > 0) {
                        got += r;
                        break;
                    }
                }
            }
            if (got == 0) return -1;
            continue;
        }

        usize needed = sz - got;
        usize deq = kqueue_dequeue(entq, buf + got, needed);
        if (deq == 0) {
            if (rng_fillpool() < 0) {
                for (usize i = 0; i < NUM_RNG_DRIVERS; i++) {
                    if (rng_drivers[i].active) {
                        int r = rng_drivers[i].read(buf + got, 1);
                        if (r > 0) {
                            deq = 1;
                            break;
                        }
                    }
                }
                if (deq == 0) return -1;
            } else {
                continue;
            }
        }

        got += deq;
    }
    return 0;
}