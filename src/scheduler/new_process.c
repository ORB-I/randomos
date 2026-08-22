#include <lib/loader.h>
#include <scheduler/process.h>

process_state_t proctbl[MAX_PROCESSES];
u8 nprocs = 0;

int new_process(const char* path, char** argv, u8 ppid) {
    if (nprocs + 1 >= MAX_PROCESSES) {
        return -1;
    }

    loadprog_res_t res = load_program(path, argv);
    if (res.status < 0) return -1;

    process_state_t* proc = &proctbl[nprocs++];
    proc->rip = res.entry;
    proc->rsp = res.rsp;
    proc->rflags = 0x202;
    proc->rax = 0;
    proc->rbx = 0;
    proc->rcx = 0;
    proc->rdx = 0;
    proc->rsi = 0;
    proc->rdi = 0;
    proc->rbp = 0;
    proc->r8 = 0;
    proc->r9 = 0;
    proc->r10 = 0;
    proc->r11 = 0;
    proc->r12 = 0;
    proc->r13 = 0;
    proc->r14 = 0;
    proc->r15 = 0;
    proc->cs = 0x1b;
    proc->ss = 0x23;
    proc->fs = 0;
    proc->gs = 0;
    proc->fsb = 0;
    proc->gsb = 0;
    proc->cr3 = (u64)res.pgtbl;
    proc->pid = nprocs-1;
    proc->is_dead = 0;
    proc->ppid = ppid;

    return proc->pid;
}
