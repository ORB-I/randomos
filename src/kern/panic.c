#include <core/panic.h>
#include <core/std.h>

#include <drivers/vga.h>

[[noreturn]] void panic(const char* msg) {
    asm("cli");
    u32 eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags;
    u16 cs, ds, es;

    asm volatile(
        "mov %%eax, %0\n\t"
        "mov %%ebx, %1\n\t"
        "mov %%ecx, %2\n\t"
        "mov %%edx, %3\n\t"
        "mov %%esi, %4\n\t"
        "mov %%edi, %5\n\t"
        "mov %%ebp, %6\n\t"
        "mov %%esp, %7\n\t"
        "mov %%cs, %8\n\t"
        "mov %%ds, %9\n\t"
        "mov %%es, %10\n\t"
        : "=m"(eax), "=m"(ebx), "=m"(ecx), "=m"(edx), "=m"(esi), "=m"(edi), "=m"(ebp), "=m"(esp), "=m"(cs), "=m"(ds), "=m"(es)
        :
        : "memory");

    eip = (u32)__builtin_return_address(0);
    asm volatile("pushf\n\t pop %0" : "=r"(eflags));

    printf("*** KERNEL PANIC ***\n%s\n\n", msg);

    printf("EAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n", eax, ebx, ecx, edx);
    printf("ESI: %08x  EDI: %08x  EBP: %08x  ESP: %08x\n", esi, edi, ebp, esp);
    printf("EIP: %08x  EFLAGS: %08x\n", eip, eflags);
    printf("CS:  %04x   DS: %04x   ES: %04x\n\n", cs, ds, es);

    printf("*** HALTING NOW ***");

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}