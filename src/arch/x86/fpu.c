#include <core/fpu.h>
#include <core/std.h>

void init_fpu() {
    // EM=0,MP=1,NE=1
    u64 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0) :: "memory");
    asm volatile("mov %0, %%cr0" :: "r"(cr0 |  0x0000000000000022) : "memory");
    asm volatile("fninit" ::: "memory");
}