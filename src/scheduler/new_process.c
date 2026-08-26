#include <lib/loader.h>
#include <scheduler/process.h>
#include <core/printf.h>
#include <core/fd.h>
#include <core/liballoc.h>
#include <lib/string.h>
#include <drivers/display/term.h>
#include <lib/string.h>
#include <drivers/hid/kbd.h>

process_state_t proctbl[MAX_PROCESSES];

ssize copy_fds(u8 dst, u8 src) {
    process_state_t* dproc = &proctbl[dst];
    process_state_t* sproc = &proctbl[src];

    struct fdinfo* new_fds = malloc(sizeof(struct fdinfo) * sproc->nfds);
    if (!new_fds) {
        return -1;
    }

    memcpy(new_fds, sproc, sizeof(struct fdinfo) * sproc->nfds);
    dproc->nfds = sproc->nfds;
    dproc->fds = sproc->fds;
    return sizeof(struct fdinfo) * sproc->nfds;
}

int kexecve(const char* path, char** argv, char** envp, u8 cpid) {
    process_state_t* proc = &proctbl[cpid];
    loadprog_res_t res = load_program(path, argv, envp);
    if (res.status < 0) {
        return -1;
    }

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
    proc->is_dead = 0;
    proc->is_blocked = 0;
    proc->wait_pid = WAIT_ANY;
    proc->wake_ms = 0;
    
    vmm_setumapbase(proc->pid, res.load_high);

    return 0;
}

static ssize _stdin_read(void* buf, usize sz) {
    for (usize i = 0; i < sz; i++) {
        *((char*)&buf[i]) = getchar();
        if (*((char*)&buf[i]) == '\n') {
            return i;
        }
    }
    return sz;
}

static ssize _stdout_write(void* buf, usize sz) {
    term_write(buf, sz);
    return sz;
}

extern framebuf_t _term_fb;
int new_process(const char* path, char** argv, char** envp, u8 ppid) {
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

    loadprog_res_t res = load_program(path, argv, envp);
    if (res.status < 0) return -1;

    if (pid == 0) {
        struct fdinfo* new_fds = malloc(sizeof(struct fdinfo) * 4);
        if (!new_fds) {
            return -1;
        }

        new_fds[0] = (struct fdinfo){
            0, 1, FDTYPE_IO, 
            .data = {.io = {1, 0, NULL, _stdin_read}}
        };

        new_fds[1] = (struct fdinfo){
            1, 1, FDTYPE_IO, 
            .data = {.io = {0, 1, _stdout_write, NULL}}
        };

        new_fds[2] = (struct fdinfo){
            2, 1, FDTYPE_IO,
            .data = {.io = {0, 1, _stdout_write, NULL}}
        };

        int cfb = get_currfb();
        struct fdinfo* info;
        if (getfd(cfb, &info) < 0) {
            return -1;
        }

        new_fds[3] = (struct fdinfo){
            3, 1, FDTYPE_FB,
            .data = {.fb = &_term_fb }
        };
        switch_fb(3); // we need to switch to 3 right away
                          // because for some reason kernel shares
                          // a state with pid1 and the same framebuffer

        proc->fds = new_fds;
        proc->nfds = 4;
        proc->currfb = 3;

        // we have to malloc this since
        // setpwd uses malloc and we will
        // need to use free() on it on process end
        proc->pwd = malloc(2);
        if (!proc->pwd) return -1;
        proc->pwd[0] = '/';
        proc->pwd[1] = '\0';
    } else {
        usize sz;
        if ((sz = copy_fds(pid, ppid)) < 0) {
            return -1;
        }
        proc->currfb = get_currfb();
        proc->pwd = strcpy(proctbl[proc->ppid].pwd);
        if (!proc->pwd) return -1;
    }

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

    if (ppid < MAX_PROCESSES && proctbl[ppid].used && pid != 0) {
        proc->uid = proctbl[ppid].uid;
        proc->gid = proctbl[ppid].gid;
        proc->euid = proctbl[ppid].euid;
        proc->egid = proctbl[ppid].egid;
    } else {
        proc->uid = 0;
        proc->gid = 0;
        proc->euid = 0;
        proc->egid = 0;
    }

    proc->used = 1;

    vmm_setumapbase(proc->pid, res.load_high);

    return proc->pid;
}