#pragma once
#include <stdatomic.h>
#include <core/std.h>

#define BSP_REQ_PANIC   1
#define BSP_REQ_SETSTAT 2
typedef struct {
    atomic_int lock;
    atomic_int done;
    u64 apicid; // senders apicid
    int type;
    void* data;
    usize data_sz;
} bsp_request_t;

#define BSP_REQVEC 255

void send_bsp_request(u64 apicid, int type, void* data, usize datasz);