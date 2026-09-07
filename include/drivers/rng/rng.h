#pragma once
#include <core/std.h>

typedef int(*random_byte_cb)(void);

typedef struct hw_rng_driver {
    const char* name;
    bool (*available)(void);
    int (*init)(void);
    int (*read)(u8* buf, usize len);
    bool active;
} hw_rng_driver_t;

u64 random64(void);
int random_bytes(u8* buf, usize sz);
int rng_init(void);