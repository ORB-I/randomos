#pragma once

#include <core/std.h>
struct sysregs {
    u64 num, a0, a1, a2, a3, a4, a5;
    u64 __es, __ds, __rflags, __rip;
};

typedef u64 (*syscall_hdlr_t)(struct sysregs* args);
#define DEFSYSCALL(NAME) u64 NAME (struct sysregs* args)