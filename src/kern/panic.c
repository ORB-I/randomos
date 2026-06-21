#include <core/panic.h>
#include <core/std.h>

#include <drivers/term.h>

[[noreturn]] void panic(const char* msg, ...) {
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

    printf("*** KERNEL PANIC ***\n");
    vprintf(msg, lst);
    printf("\n\n");

    va_end(lst);

    printf("RAX: %016x  RBX: %016x  RCX: %016x  RDX: %016x\n", rax, rbx, rcx, rdx);
    printf("RSI: %016x  RDI: %016x  RBP: %016x  RSP: %016x\n", rsi, rdi, rbp, rsp);
    printf("RIP: %016x  RFLAGS: %016x\n", rip, rflags);
    printf("CS:  %04x   DS: %04x   ES: %04x\n\n", cs, ds, es);

    printf("*** HALTING NOW ***");

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}