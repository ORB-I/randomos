#include <core/debug.h>
#include <core/panic.h>
#include <core/std.h>
#include <core/printf.h>
#include <core/mem/vmm.h>
#include <core/idt.h>
#include <drivers/display/term.h>
#include <drivers/display/fb.h>
#include <drivers/display/serial.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <lib/loader.h>

extern __attribute__((aligned(16))) u8 kern_stack[16384];

struct CpuState {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 intr_no, error_code;
    u64 rip, cs, rflags, rsp, ss;
} __attribute__((packed));

// kill the faulting user process and hand control back to the scheduler.
// if no processes remain, fall through to the kernel panic path below.
static void kill_user_process(struct CpuState* regs, const char* msg, va_list lst) {
    asm volatile("swapgs" ::: "memory");

    // one pass to the framebuffer terminal, one to serial; copies because
    // each vprintf() burns its own va_list
    va_list flst, slst;
    va_copy(flst, lst);
    va_copy(slst, lst);

    printf("user fault (pid %d): ", current_pid);
    vprintf(msg, flst);
    printf("\nRIP=%016lx  RBP=%016lx RSP=%016lx  CS=%04lx\n",
           regs->rip, regs->rbp, regs->rsp, regs->cs);
    va_end(flst);

    serial_printf("user fault (pid %d): ", current_pid);
    serial_vprintf(msg, slst);
    serial_printf("\nRIP=%016lx  RBP=%016lx RSP=%016lx  CS=%04lx\n",
                  regs->rip, regs->rbp, regs->rsp, regs->cs);
    va_end(slst);

    page_table_t* uasp = vmm_cpml4v();
    vmm_skasp();

    proctbl[current_pid].is_dead = 1;
    reparent_children(current_pid);
    wake_waiter(current_pid);

    if (current_pid == 0) {
        panic("pid0 exited");
    }

    // context is garbage — process is dead, so what we save doesn't matter
    asm volatile("cli");
    procctx_t abandoned = {0};
    scheduler_switch(&abandoned);

    // scheduler_switch returns only when nothing is left to run
    vmm_remumap(current_pid, uasp);
    vmm_dasp(uasp);
    panic("all processes have exited");
}

void backtrace(u64 rbp) {
    if (!rbp) return;
    for (usize i = 0; i < 10; i++) {
        u64* fp = (u64*)rbp;
        if (!vmm_get_phys(vmm_cpml4v(), rbp)) break;
        rbp = fp[0];
        struct kern_symbol* sym = locate_symbol(fp[1]);
        const char* syms = (sym) ? sym->name : "unknown";
        printf("%d: %s (%p)\n", i, syms, fp[1]);
        serial_printf("%d: %s (%p)\n", i, syms, fp[1]);
        if (!rbp) break;
    }
}

void except_panic(struct CpuState* regs, const char* msg, ...) {
    asm("cli");
    // one pass to the framebuffer terminal, one to serial for headless
    // debugging; va_copy keeps the second vprintf() legal
    va_list lst, slst;
    va_start(lst, msg);
    va_copy(slst, lst);

    // user-mode fault: kill the process, don't take down the kernel
    if ((regs->cs & 0x3) == 3) {
        kill_user_process(regs, msg, lst);
        // never reached unless all processes are dead (panic above)
        return;
    }

    printf("*** KERNEL EXCEPTION ***\n");
    vprintf(msg, lst);
    printf("\n\n");

    printf("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    printf("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    printf("RIP: %016lx  RFLAGS: %016lx\n", regs->rip, regs->rflags);
    printf("ERR: %016lx  INTR: %016lx\n", regs->error_code, regs->intr_no);
    printf("CS:  %016lx  SS: %016lx\n\n", regs->cs, regs->ss);

    serial_puts("*** KERNEL EXCEPTION ***\n");
    serial_vprintf(msg, slst);
    serial_puts("\n\n");

    serial_printf("RAX: %016lx  RBX: %016lx  RCX: %016lx  RDX: %016lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    serial_printf("RSI: %016lx  RDI: %016lx  RBP: %016lx  RSP: %016lx\n", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    serial_printf("RIP: %016lx  RFLAGS: %016lx\n", regs->rip, regs->rflags);
    serial_printf("ERR: %016lx  INTR: %016lx\n", regs->error_code, regs->intr_no);
    serial_printf("CS:  %016lx  SS: %016lx\n", regs->cs, regs->ss);
    backtrace(regs->rbp);

    serial_puts("\n*** HALTING NOW ***\n");
    printf("\n*** HALTING NOW ***\n");

    va_end(lst);
    va_end(slst);
    asm volatile("cli");
    while (1) asm volatile("hlt");
}

void c_int_hdlr(struct CpuState* regs) {
    struct kern_symbol* sym = locate_symbol(regs->rip);
    const char* syms = (sym) ? sym->name : "unknown";
    switch (regs->intr_no) {
        case 0:  except_panic(regs, "Division Error (at %s)", syms); break;
        case 1:  except_panic(regs, "Debug Excepttion (at %s)", syms); break;
        case 3:  except_panic(regs, "Breakpoint reached (at %s)", syms); break;
        case 4:  except_panic(regs, "Overflow Exception (at %s)", syms); break;
        case 5:  except_panic(regs, "BOUND range exceeded (at %s)", syms); break;
        case 6:  except_panic(regs, "Invalid Opcode (at %s)", syms); break;
        case 7:  except_panic(regs, "Device not available (at %s)", syms); break;
        case 8:  except_panic(regs, "Double fault (at %s)", syms); break;
        case 10: except_panic(regs, "Invalid TSS (at %s)", syms); break;
        case 11: except_panic(regs, "Segment doesn't exist (at %s)", syms); break;
        case 12: except_panic(regs, "Stack fault (at %s)", syms); break;
        case 13: except_panic(regs, "General protection fault (at %s)", syms); break;
        case 14: {
            u64 badaddr;
            u32 ec = regs->error_code;
            asm volatile("mov %%cr2, %0" : "=r"(badaddr));
            except_panic(regs, "Page fault on address 0x%016x (%s %s %s %s %s)",
                badaddr,
                (ec & (1 << 0)) ? "Present" : "Not-Present",
                (ec & (1 << 1)) ? "Write" : "Read",
                (ec & (1 << 2)) ? "User" : "Supervisor",
                (ec & (1 << 4)) ? "Instruction-Fetch" : "Access",
                syms
            );
            break;
        }
        case 17: except_panic(regs, "Alignment check fault (at %s)", syms); break;
        default: except_panic(regs, "Unhandled Exception: %d at %s", regs->intr_no, syms);
    }
}

