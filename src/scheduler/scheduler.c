#include <scheduler/process.h>
#include <lib/syscall.h>
#include <core/mem/vmm.h>

// these arent defined
// in a header because theyre only
// meant to be used by the scheduler
typedef struct {
    u64 time;
    u8 irq;
    u8 timerid;
} preemptive_timer_t;
int hpet_mkpreemptive_timer(preemptive_timer_t* buf, u64 ms, void(*hdlr)(void));
int hpet_start_preemptive(preemptive_timer_t* timer);
int hpet_active();

preemptive_timer_t _schdlr_timer;
u8 current_pid = 0;

extern void preempt_hdlr();

int init_scheduler() {
    if (!hpet_active()) return -1;
    hpet_mkpreemptive_timer(&_schdlr_timer, 20, preempt_hdlr);
    return 0;
}

void proc2ctx(procctx_t* dst, process_state_t* src) {
    dst->rip = src->rip; dst->rsp = src->rsp; dst->rflags = src->rflags;
    dst->rax = src->rax; dst->rbx = src->rbx; dst->rcx = src->rcx;
    dst->rdx = src->rdx; dst->rsi = src->rsi; dst->rdi = src->rdi;
    dst->rbp = src->rbp; dst->r8 = src->r8; dst->r9 = src->r9;
    dst->r10 = src->r10; dst->r11 = src->r11; dst->r12 = src->r12;
    dst->r13 = src->r13; dst->r14 = src->r14; dst->r15 = src->r15;
    dst->cs = src->cs; dst->ss = src->ss; dst->fs = src->fs;
    dst->gs = src->gs; dst->fsb = src->fsb; dst->cr3 = src->cr3;
}

void ctx2proc(process_state_t* dst, procctx_t* src) {
    dst->rip = src->rip; dst->rsp = src->rsp; dst->rflags = src->rflags;
    dst->rax = src->rax; dst->rbx = src->rbx; dst->rcx = src->rcx;
    dst->rdx = src->rdx; dst->rsi = src->rsi; dst->rdi = src->rdi;
    dst->rbp = src->rbp; dst->r8 = src->r8; dst->r9 = src->r9;
    dst->r10 = src->r10; dst->r11 = src->r11; dst->r12 = src->r12;
    dst->r13 = src->r13; dst->r14 = src->r14; dst->r15 = src->r15;
    dst->cs = src->cs; dst->ss = src->ss; dst->fs = src->fs;
    dst->gs = src->gs; dst->fsb = src->fsb; dst->cr3 = src->cr3;
}

[[noreturn]] void start_scheduler() {
    process_state_t* proc = &proctbl[current_pid];
    init_syscalls(); // we reset ts every time cuz
                    // if i dont syscalls break idk why
    procctx_t ctx;
    proc2ctx(&ctx, proc);

    vmm_sasp((page_table_t*)proc->cr3);
    asm volatile(
        "cli\n\t"
        "movq %[ctx], %%15\n\t"
        "movq 0x00(%%r15), %%rax\n\t"
        "movq 0x08(%%r15), %%rbx\n\t"
        "movq 0x10(%%r15), %%rcx\n\t"
        "movq 0x18(%%r15), %%rdx\n\t"
        "movq 0x20(%%r15), %%rsi\n\t"
        "movq 0x28(%%r15), %%rdi\n\t"
        "movq 0x30(%%r15), %%rbp\n\t"
        "movq 0x38(%%r15), %%r8\n\t"
        "movq 0x40(%%r15), %%r9\n\t"
        "movq 0x48(%%r15), %%r10\n\t"
        "movq 0x50(%%r15), %%r11\n\t"
        "movq 0x58(%%r15), %%r12\n\t"
        "movq 0x60(%%r15), %%r13\n\t"
        "movq 0x68(%%r15), %%r14\n\t"
        "pushq 0x92(%%r15)\n\t"
        "pushq 0x08(%%r15)\n\t"
        "pushq 0x10(%%r15)\n\t"
        "pushq 0x90(%%r15)\n\t"
        "pushq 0x00(%%r15)\n\t"

        "movq 0x70(%%r15), %%r15\n\t"
        "iretq\n\t"
        :
        : [ctx] "r"(ctx)
        : "memory"
    );

    __builtin_unreachable();
}

u8 nextproc() {
    if (current_pid + 1 >= nprocs) {
        return 0;
    } else {
        return current_pid + 1;
    }
}

void scheduler_switch(procctx_t* proc) {
    u8 tgtpid = nextproc();

    process_state_t* tgtproc = &proctbl[tgtpid];
    process_state_t* currproc = &proctbl[current_pid];

    while (tgtproc->is_dead) { // get a non-dead process cuz
                               // if a process is dead and we try to switch to it
                               // chances are its RIP or RIP+1 is invalid unless it was signalled
                               // in which case its likely that the process is broken or in an
                               // illegal state
        tgtpid = nextproc();
        tgtproc = &proctbl[tgtpid];
    }

    ctx2proc(currproc, proc);

    init_syscalls();
    procctx_t ctx;
    proc2ctx(&ctx, tgtproc);
    vmm_sasp((page_table_t*)proc->cr3);
    asm volatile(
        "cli\n\t"
        "movq %[ctx], %%15\n\t"
        "movq 0x00(%%r15), %%rax\n\t"
        "movq 0x08(%%r15), %%rbx\n\t"
        "movq 0x10(%%r15), %%rcx\n\t"
        "movq 0x18(%%r15), %%rdx\n\t"
        "movq 0x20(%%r15), %%rsi\n\t"
        "movq 0x28(%%r15), %%rdi\n\t"
        "movq 0x30(%%r15), %%rbp\n\t"
        "movq 0x38(%%r15), %%r8\n\t"
        "movq 0x40(%%r15), %%r9\n\t"
        "movq 0x48(%%r15), %%r10\n\t"
        "movq 0x50(%%r15), %%r11\n\t"
        "movq 0x58(%%r15), %%r12\n\t"
        "movq 0x60(%%r15), %%r13\n\t"
        "movq 0x68(%%r15), %%r14\n\t"
        "pushq 0x92(%%r15)\n\t"
        "pushq 0x08(%%r15)\n\t"
        "pushq 0x10(%%r15)\n\t"
        "pushq 0x90(%%r15)\n\t"
        "pushq 0x00(%%r15)\n\t"

        "movq 0x70(%%r15), %%r15\n\t"
        "iretq\n\t"
        :
        : [ctx] "r"(ctx)
        : "memory"
    );

    __builtin_unreachable();
}
