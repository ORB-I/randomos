#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <core/panic.h>
#include <core/liballoc.h>
#include <core/mem/vmm.h>
#include <core/errno.h>
#include "../ssc.h"

void endproc_shared(s64 pid) {
    asm volatile("cli");
    proctbl[pid].is_blocked = 0;
    proctbl[pid].is_dead = 1;
    reparent_children((u8)pid);
    wake_waiter((u8)pid);

    free(proctbl[pid].fds);
    proctbl[pid].fds = NULL;
    free(proctbl[pid].pwd);
    proctbl[pid].pwd = NULL;

    vmm_dasp((page_table_t*)proctbl[pid].cr3);
    proctbl[pid].used = 0;
}

DEFSYSCALL(sys_kill) {
    s64 pid = args->a0;

    if (pid == 0) return -EINVAL; // dont kill init, that could panic kernel
    if (pid <= 0 || pid >= MAX_PROCESSES || pid == current_pid ||
        !proctbl[pid].used || proctbl[pid].is_dead) {
        return -ENOPROC;
    }

    if (proctbl[current_pid].euid != 0 &&
        proctbl[current_pid].euid != proctbl[pid].uid &&
        proctbl[current_pid].uid != proctbl[pid].uid) {
        return -EACCESS;
    }

    endproc_shared(pid);

    return 0;
}

DEFSYSCALL(sys_exit) {
    int code = args->a0;
    vmm_skasp();
    proctbl[current_pid].code = code;
    proctbl[current_pid].is_dead = 1;

    endproc_shared(current_pid);

    procctx_t abandoned = {0};
    asm("sti");
    scheduler_switch(&abandoned);

    // scheduler_switch only returns when nobody is left
    panic("all processes have exited");
}