#include <core/panic.h>
#include <core/std.h>

struct CpuState {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 esi;
    u32 edi;
    u32 ebp;
    u32 esp;
} __attribute__((packed));

struct StackState {
    u32 error_code;
    u32 eip;
    u32 cs;
    u32 eflags;
} __attribute__((packed));

void c_int_hdlr(struct CpuState cpu, struct StackState stack, u32 intr) {
    (void)cpu;
    (void)stack;

    switch (intr) {
        case 8:
            panic("Double fault");
        case 10:
            panic("Invalid TSS");
        case 11:
            panic("Segment doesn't exist");
        case 12:
            panic("Stack fault");
        case 13:
            panic("General protection fault");
        case 14:
            panic("Page fault");
        case 17:
            panic("Alignment check fault");
    }
}