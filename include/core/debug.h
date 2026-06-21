#pragma once
#include <core/std.h>

struct kern_symbol {
    u64 addr;
    const char* name;
};

extern struct kern_symbol ksymtbl[];
extern usize nksyms;

struct kern_symbol* locate_symbol(u64 rip);