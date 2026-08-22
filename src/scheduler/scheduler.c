#include <scheduler/process.h>
#include <lib/syscall.h>
#include <core/mem/vmm.h>
#include <lib/loader.h>
#include <core/asmh.h>

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

u8 preempt_pending = 0;
procctx_t preempt_ctx;

int init_scheduler() {
    if (!hpet_active()) return -1;
    hpet_mkpreemptive_timer(&_schdlr_timer, 20, preempt_hdlr);
    return 0; // dont start the preempt timer yet because we dont have processes yet, start_scheduler should do that
}

void proc2ctx(procctx_t* dst, process_state_t* src) {
    dst->rip = src->rip; dst->rsp = src->rsp; dst->rflags = src->rflags;
    dst->rax = src->rax; dst->rbx = src->rbx; dst->rcx = src->rcx;
    dst->rdx = src->rdx; dst->rsi = src->rsi; dst->rdi = src->rdi;
    dst->rbp = src->rbp; dst->r8 = src->r8; dst->r9 = src->r9;
    dst->r10 = src->r10; dst->r11 = src->r11; dst->r12 = src->r12;
    dst->r13 = src->r13; dst->r14 = src->r14; dst->r15 = src->r15;
    dst->cs = src->cs; dst->ss = src->ss; dst->fs = src->fs;
    dst->gs = src->gs; dst->fsb = src->fsb; dst->gsb = src->gsb;
    dst->cr3 = src->cr3;
}

void ctx2proc(process_state_t* dst, procctx_t* src) {
    dst->rip = src->rip; dst->rsp = src->rsp; dst->rflags = src->rflags;
    dst->rax = src->rax; dst->rbx = src->rbx; dst->rcx = src->rcx;
    dst->rdx = src->rdx; dst->rsi = src->rsi; dst->rdi = src->rdi;
    dst->rbp = src->rbp; dst->r8 = src->r8; dst->r9 = src->r9;
    dst->r10 = src->r10; dst->r11 = src->r11; dst->r12 = src->r12;
    dst->r13 = src->r13; dst->r14 = src->r14; dst->r15 = src->r15;
    dst->cs = src->cs; dst->ss = src->ss; dst->fs = src->fs;
    dst->gs = src->gs; dst->fsb = src->fsb; dst->gsb = src->gsb;
    dst->cr3 = src->cr3;
}

[[noreturn]] void switch_ctx(procctx_t* ctx);
[[noreturn]] void start_scheduler() {
    process_state_t* proc = &proctbl[current_pid];
    procctx_t ctx;
    proc2ctx(&ctx, proc);

    reset_kgsb();
    vmm_sasp((page_table_t*)proc->cr3);
    switch_ctx(&ctx);
}

// next alive process after current_pid, or -1 when everything is dead
int nextproc() {
    if (nprocs == 0) return -1;

    u8 start = current_pid;
    u8 pid = current_pid;
    do {
        pid = (pid + 1) % nprocs;
        if (!proctbl[pid].is_dead) {
            return pid;
        }
    } while (pid != start);
    return -1;
}

void scheduler_switch(procctx_t* proc) {
    int tgtpid = nextproc();

    if (tgtpid < 0 || tgtpid == (int)current_pid) {
        return;
    }

    process_state_t* tgtproc = &proctbl[tgtpid];
    process_state_t* currproc = &proctbl[current_pid];
    u64 curcr3 = currproc->cr3;

    ctx2proc(currproc, proc);
    current_pid = (u8)tgtpid;

    hpet_start_preemptive(&_schdlr_timer);
    reset_kgsb();
    procctx_t ctx;
    proc2ctx(&ctx, tgtproc);
    vmm_sasp((page_table_t*)tgtproc->cr3);

    // whoever exited before the switch doesn't need
    // their address space anymore
    if (currproc->is_dead) {
        vmm_dasp((page_table_t*)curcr3);
    }
    
    switch_ctx(&ctx);
}
