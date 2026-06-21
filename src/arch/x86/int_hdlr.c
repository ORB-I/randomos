#include "core/debug.h"
#include <core/panic.h>
#include <core/std.h>
#include <drivers/term.h>

struct CpuState {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 intr_no, error_code;
    u64 rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void c_int_hdlr(struct CpuState* regs) {
    struct kern_symbol* sym = locate_symbol(regs->rip);
    const char* syms = (sym) ? sym->name : "unknown";
    switch (regs->intr_no) {
        case 8:  panic("Double fault (at %s)", syms);
        case 10: panic("Invalid TSS (at %s)", syms);
        case 11: panic("Segment doesn't exist (at %s)", syms);
        case 12: panic("Stack fault (at %s)", syms);
        case 13: panic("General protection fault (at %s)", syms);
        case 14: {
            u64 badaddr;
            u32 ec = regs->error_code;
            asm volatile("mov %%cr2, %0" : "=r"(badaddr));
            panic("Page fault on address 0x%016x (%s %s %s %s %s)\n", 
                badaddr,
                (ec & (1 << 0)) ? "Present" : "Not-Present",
                (ec & (1 << 1)) ? "Write" : "Read",
                (ec & (1 << 2)) ? "User" : "Supervisor",
                (ec & (1 << 4)) ? "Instruction-Fetch" : "Access",
                syms
            );
        }
        case 17: panic("Alignment check fault (at %s)", syms);
        default: panic("Unhandled Exception: %d at %s\n", regs->intr_no, syms);
    }
}