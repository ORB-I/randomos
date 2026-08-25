#pragma once
#include <stdatomic.h>
#include <core/std.h>

#define AP_REQ_RUN    0x00
#define AP_REQ_PAUSE  0x01
#define AP_REQ_CONT   0x02
#define AP_REQ_STOP   0x03

typedef struct {
    void (*fn)(void*);
    void* arg;
} __attribute__((packed)) ap_runreq_t;

typedef struct {
    atomic_int lock;
    atomic_int done;
    u64 apicid; // receiver apic id
    int type;
    void* data; // for run should contain a pointer to an ap_runreq_t
    usize data2;
} ap_req_t;

#define AP_WAITING 0x00
#define AP_PAUSED  0x01
#define AP_START   0x02
#define AP_RUNNING 0x03
typedef struct {
    _Alignas(4) atomic_int lock; // has to be aligned or else clang will try to call __atomic_*

    int state;
    ap_runreq_t rreq;

    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;

    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __attribute__((packed)) ap_state;

extern ap_state* apstates;
extern ap_req_t* apreqvec;

#define AP_STOPVEC 254 // an interrupt on this vector
                       // will immediatly halt the processor indefinitely
#define AP_MSGVEC  255