#include <core/panic.h>
#include <core/std.h>
#include <core/printf.h>
#include <drivers/display/serial.h>

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

    // one pass to the framebuffer terminal, one to serial for headless
    // debugging; va_copy keeps the second vprintf() legal
    va_list lst, slst;
    va_start(lst, msg);
    va_copy(slst, lst);

    printf("*** KERNEL PANIC ***\n");
    vprintf(msg, lst);
    printf("\n\n");
    va_end(lst);

    serial_puts("*** KERNEL PANIC ***\n");
    serial_vprintf(msg, slst);
    serial_puts("\n\n");
    va_end(slst);

    printf("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", rax, rbx, rcx, rdx);
    printf("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", rsi, rdi, rbp, rsp);
    printf("RIP: %016lx  RFLAGS: %016lx\n", rip, rflags);
    printf("CS:  %04x   DS: %04x   ES: %04x\n\n", cs, ds, es);

    serial_printf("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", rax, rbx, rcx, rdx);
    serial_printf("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", rsi, rdi, rbp, rsp);
    serial_printf("RIP: %016lx  RFLAGS: %016lx\n", rip, rflags);
    serial_printf("CS:  %04x   DS: %04x   ES: %04x\n\n", cs, ds, es);

    serial_puts("*** HALTING NOW ***\n");

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}