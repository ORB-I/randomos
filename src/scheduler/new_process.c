#include <lib/loader.h>
#include <scheduler/process.h>
#include <core/printf.h>

process_state_t proctbl[MAX_PROCESSES];

int new_process(const char* path, char** argv, u8 ppid) {
    process_state_t* proc = NULL;
    u8 pid = 0;
    for (usize i = 0; i < MAX_PROCESSES; i++) {
        if (!proctbl[i].used) {
            proc = &proctbl[i];
            pid = i;
            break;
        }
    }
    if (!proc) return -1;

    loadprog_res_t res = load_program(path, argv);
    if (res.status < 0) return -1;

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
    proc->pid = pid;
    proc->is_dead = 0;
    proc->ppid = ppid;
    proc->is_blocked = 0;
    proc->wait_pid = WAIT_ANY;
    proc->wake_ms = 0;
    proc->used = 1;

    vmm_setumapbase(proc->pid, res.load_high);

    return proc->pid;
}
