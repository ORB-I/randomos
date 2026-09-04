#include <core/panic.h>
#include <core/std.h>
#include <core/kprint.h>
#include <core/debug.h>
#include <drivers/display/serial.h>

__noreturn __no_protect void panic(const char* msg, ...) {
    asm("cli");
    
    u64 rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, rip, rflags;
    u16 cs, ds, es;

    asm volatile(
        "mov %%rax, %0\n\t"
        "mov %%rbx, %1\n\t"
        "mov %%rcx, %2\n\t"
        "mov %%rdx, %3\n\t"
        "mov %%rsi, %4\n\t"
        "mov %%rdi, %5\n\t"
        "mov %%rbp, %6\n\t"
        "mov %%rsp, %7\n\t"
        "mov %%cs, %8\n\t"
        "mov %%ds, %9\n\t"
        "mov %%es, %10\n\t"
        : "=m"(rax), "=m"(rbx), "=m"(rcx), 
          "=m"(rdx), "=m"(rsi), "=m"(rdi), 
          "=m"(rbp), "=m"(rsp), "=m"(cs), 
          "=m"(ds), "=m"(es)
        :: "memory"
    );

    rip = (u64)__builtin_return_address(0);
    asm volatile("pushf\n\t pop %0" : "=r"(rflags));

    va_list lst;
    va_start(lst, msg);

    kprint("*** KERNEL PANIC ***\n");
    kvprint(msg, lst);
    kprint("\n\n");
    va_end(lst);

    kprint("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", rax, rbx, rcx, rdx);
    kprint("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", rsi, rdi, rbp, rsp);
    kprint("RIP: %016lx  RFLAGS: %016lx\n", rip, rflags);
    kprint("CS:  %04x   DS: %04x   ES: %04x\n\n", cs, ds, es);

    backtrace(rbp);

    kprint("\n*** HALTING NOW ***\n");

    asm volatile("cli");
    while (1) { asm volatile("hlt"); }
}