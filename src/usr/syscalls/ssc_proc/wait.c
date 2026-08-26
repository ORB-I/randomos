#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <core/mem/vmm.h>
#include "../ssc.h"

// whoever was parked in SYS_WAIT on pid (or was waiting for any child)
// gets to run again, rax doubles as their wait return value so it gets
// handed the dying childs pid
void wake_waiter(u8 pid) {
    process_state_t* parent = &proctbl[proctbl[pid].ppid];
    if (parent->is_blocked && (parent->wait_pid == pid ||
                               parent->wait_pid == WAIT_ANY)) {
        parent->is_blocked = 0;
        parent->rax = pid;
        page_table_t* cr3 = vmm_cpml4v();
        vmm_sasp((page_table_t*)parent->cr3);
        if (parent->codeptr) *parent->codeptr = proctbl[pid].code;
        vmm_sasp(cr3);
    }
}

// reparent orphan children of an exiting or killed process to init (pid 0)
void reparent_children(u8 old_ppid) {
    for (u8 i = 0; i < MAX_PROCESSES; i++) {
        if (proctbl[i].used && !proctbl[i].is_dead && proctbl[i].ppid == old_ppid) {
            proctbl[i].ppid = 0;
        }
    }
}

DEFSYSCALL(sys_wait) {
    s64 pid = args->a0;
    int* code = (void*)(uintptr_t)args->a1;
    if (pid >= 0) {
        if (pid >= MAX_PROCESSES || !proctbl[pid].used || pid == current_pid) return -1;
        if (!proctbl[pid].is_dead) {
            proctbl[current_pid].wait_pid = (u8)pid;
            proctbl[current_pid].is_blocked = 1;
            proctbl[current_pid].codeptr = code;
            preempt_pending = 1;
            if (code) *code = -1;
        } else {
            proctbl[pid].used = 0;
            if (code) *code = proctbl[pid].code;
        }
        return pid;
    } else {
        for (usize i = 0; i < MAX_PROCESSES; i++) {
            if (i != current_pid && proctbl[i].ppid == current_pid && proctbl[i].is_dead) {
                proctbl[i].used = 0;
                if (code) *code = proctbl[pid].code;
                return i;
            }
        }

        // if no processes are dead we should hang the process
        proctbl[current_pid].wait_pid = WAIT_ANY;
        proctbl[current_pid].is_blocked = 1;
        proctbl[current_pid].codeptr = code;
        return -1;
    }
}