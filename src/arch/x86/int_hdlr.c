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

[[noreturn]] void except_panic(struct CpuState* regs, const char* msg, ...) {
    asm("cli");
    
    va_list lst;
    va_start(lst, msg);

    printf("*** KERNEL EXCEPTION ***\n");
    vprintf(msg, lst);
    printf("\n\n");
    
    va_end(lst);

    printf("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    printf("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    printf("RIP: %016lx  RFLAGS: %016lx\n", regs->rip, regs->rflags);
    printf("ERR: %016lx  INTR: %016lx\n", regs->error_code, regs->intr_no);
    printf("CS:  %016lx  SS: %016lx\n\n", regs->cs, regs->ss);

    printf("*** HALTING NOW ***");

    asm volatile("cli");
    while (1) asm volatile("hlt");
}

void c_int_hdlr(struct CpuState* regs) {
    struct kern_symbol* sym = locate_symbol(regs->rip);
    const char* syms = (sym) ? sym->name : "unknown";
    switch (regs->intr_no) {
        case 8:  except_panic(regs, "Double fault (at %s)", syms);
        case 10: except_panic(regs, "Invalid TSS (at %s)", syms);
        case 11: except_panic(regs, "Segment doesn't exist (at %s)", syms);
        case 12: except_panic(regs, "Stack fault (at %s)", syms);
        case 13: except_panic(regs, "General protection fault (at %s)", syms);
        case 14: {
            u64 badaddr;
            u32 ec = regs->error_code;
            asm volatile("mov %%cr2, %0" : "=r"(badaddr));
            except_panic(regs, "Page fault on address 0x%016x (%s %s %s %s %s)\n", 
                badaddr,
                (ec & (1 << 0)) ? "Present" : "Not-Present",
                (ec & (1 << 1)) ? "Write" : "Read",
                (ec & (1 << 2)) ? "User" : "Supervisor",
                (ec & (1 << 4)) ? "Instruction-Fetch" : "Access",
                syms
            );
        }
        case 17: except_panic(regs, "Alignment check fault (at %s)", syms);
        default: except_panic(regs, "Unhandled Exception: %d at %s\n", regs->intr_no, syms);
    }
}