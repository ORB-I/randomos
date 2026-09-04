#include <scheduler/process.h>
#include <lib/syscall.h>
#include <core/mem/vmm.h>
#include <lib/loader.h>
#include <core/asmh.h>
#include <drivers/time/clock.h>
#include <core/kprint.h>
#include <drivers/display/fb.h>
#include <drivers/storage/fs.h>
#include <core/errno.h>
#include <smp/smp.h>

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
void hpet_pause_preemptive(preemptive_timer_t* timer);
int hpet_active();

preemptive_timer_t _schdlr_timer;
spinlock_t proctbl_lock = SPINLOCK_INIT;
u8 current_pid = 0;

extern void preempt_hdlr();

u8 preempt_pending = 0;
procctx_t preempt_ctx;

int init_scheduler() {
    if (!hpet_active()) return -EINVAL;
    hpet_mkpreemptive_timer(&_schdlr_timer, 100, preempt_hdlr);
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
}

[[noreturn]] void switch_ctx(procctx_t* ctx);
[[noreturn]] void start_scheduler() {
    spinlock_acquire(&proctbl_lock);
    process_state_t* proc = &proctbl[current_pid];
    procctx_t ctx;
    proc2ctx(&ctx, proc);
    spinlock_release(&proctbl_lock);
    //kprint("Switching to %d\n", current_pid);

    hpet_start_preemptive(&_schdlr_timer);
    reset_kgsb();
    vmm_sasp((page_table_t*)proc->cr3);
    
    switch_fb(proc->currfb);
    switch_ctx(&ctx);
}

// next runnable process after current_pid (dead and blocked ones dont
// count), or -1 when there is nobody left to run
int nextproc() {
    int pid = -1;
    spinlock_acquire(&proctbl_lock);
    int start = (int)current_pid;
    pid = start;
    do {
        pid = (pid + 1) % MAX_PROCESSES;
        if (proctbl[pid].used && !proctbl[pid].is_dead) {
            int in_use = 0;
            if (smp_info) {
                for (usize c = 0; c < ncores; c++) {
                    if (smp_info[c].apicid != bsp_apicid && smp_info[c].current_pid == (u8)pid) {
                        in_use = 1;
                        break;
                    }
                }
            }
            if (in_use) continue;

            if (proctbl[pid].is_blocked) {
                // If it's a sleep timer, check if it expired
                if (proctbl[pid].wake_ms != 0 && getms && getms() >= proctbl[pid].wake_ms) {
                    proctbl[pid].is_blocked = 0;
                    proctbl[pid].wake_ms = 0;
                    spinlock_release(&proctbl_lock);
                    return pid;
                }
            } else {
                spinlock_release(&proctbl_lock);
                return pid;
            }
        } else if (proctbl[pid].used) {
        } else {
        }
    } while (pid != start);
    spinlock_release(&proctbl_lock);
    return -ENOPROC;
}

int scheduler_execve = 0;
void krunpolls();
void scheduler_switch(procctx_t* proc) {
    hpet_pause_preemptive(&_schdlr_timer);
    preempt_pending = 0;
    
    krunpolls();

    int tgtpid = nextproc();

    while (tgtpid < 0) {
        // If all processes are blocked or sleeping, halt until next timer tick
        hpet_start_preemptive(&_schdlr_timer);
        asm volatile(
            "sti\n\t"
            "hlt\n\t"
            "cli\n\t"
        );
        hpet_pause_preemptive(&_schdlr_timer);
        tgtpid = nextproc();
    }

    spinlock_acquire(&proctbl_lock);
    if (tgtpid == (int)current_pid && !proctbl[current_pid].is_dead) {
        if (scheduler_execve) {
            scheduler_execve = 0;
        } else {
            spinlock_release(&proctbl_lock);
            hpet_start_preemptive(&_schdlr_timer);
            return;
        }
    }

    process_state_t* tgtproc = &proctbl[tgtpid];
    process_state_t* currproc = &proctbl[current_pid];

    ctx2proc(currproc, proc);
    current_pid = (u8)tgtpid;
    if (smp_info) {
        for (usize c = 0; c < ncores; c++) {
            if (smp_info[c].apicid == bsp_apicid) {
                smp_info[c].current_pid = (u8)tgtpid;
                break;
            }
        }
    }
    spinlock_release(&proctbl_lock);

    reset_kgsb();
    procctx_t ctx;
    proc2ctx(&ctx, tgtproc);
    vmm_sasp((page_table_t*)tgtproc->cr3);
    chdir(tgtproc->pwd);
    
    switch_fb(tgtproc->currfb);

    // we have no idea how long the previous stuff took
    // and _schdlr_timer is mapped to all address spaces the same
    // so its safer to start the preempt timer here
    hpet_start_preemptive(&_schdlr_timer);
    switch_ctx(&ctx);
}