#include <sys/syscall.h>
#include <sys/types.h>
#include <stdarg.h>

u32 syscall(u32 nr, ...) {
    va_list lst;
    va_start(lst, nr);

    register u32 eax asm("eax") = nr;
    register u32 ebx asm("ebx") = va_arg(lst, u32);
    register u32 ecx asm("ecx") = va_arg(lst, u32);
    register u32 edx asm("edx") = va_arg(lst, u32);
    register u32 edi asm("edi") = va_arg(lst, u32);
    register u32 esi asm("esi") = va_arg(lst, u32);

    va_end(lst);

    u32 ret;

    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "r"(eax), "r"(ebx), "r"(ecx), "r"(edx), "r"(edi), "r"(esi)
        : "memory"
    );

    return ret;
}