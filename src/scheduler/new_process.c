#include <lib/loader.h>
#include <scheduler/process.h>
#include <core/kprint.h>
#include <core/fd.h>
#include <core/liballoc.h>
#include <lib/string.h>
#include <drivers/display/term.h>
#include <lib/string.h>
#include <drivers/hid/kbd.h>
#include <core/errno.h>

process_state_t proctbl[MAX_PROCESSES];

ssize copy_fds(u8 dst, u8 src) {
    process_state_t* dproc = &proctbl[dst];
    process_state_t* sproc = &proctbl[src];

    if (!sproc->fds || sproc->nfds == 0) return -EBADF;

    struct fdinfo* new_fds = malloc(sizeof(struct fdinfo) * sproc->nfds);
    if (!new_fds) {
        return -ENOMEM;
    }

    // the child gets its OWN copy of the table; sharing the parent's
    // pointer means the first exiting child frees it out from under
    // everyone still running (fds pages get unmapped by liballoc)
    memcpy(new_fds, sproc->fds, sizeof(struct fdinfo) * sproc->nfds);
    dproc->nfds = sproc->nfds;
    dproc->fds = new_fds;
    return sizeof(struct fdinfo) * sproc->nfds;
}

int kexecve(const char* path, char** argv, char** envp, u8 cpid) {
    kprint("New process %s requested\n", path);
    process_state_t* proc = &proctbl[cpid];

    loadprog_res_t res = load_program(path, argv, envp);
    if (res.status < 0) {
        kprint("failed to load program with code %d\n", res.status);
        return res.status;
    }

    struct stat st;
    int ret = 0;
    if ((ret = stat(path, &st)) < 0) {
        return ret;
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
    proc->perms = 0;

    if (st.st_mode & S_ISUID) {
        proc->perms |= PROC_SUID;
    }

    if (st.st_mode & S_ISGID) {
        proc->perms |= PROC_SGID;
    }
    
    vmm_setumapbase(proc->pid, res.load_high);
    kprint("New process %s created\n", path);
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

    spinlock_acquire(&proctbl_lock);
    for (usize i = 0; i < MAX_PROCESSES; i++) {
        if (!proctbl[i].used) {
            proc = &proctbl[i];
            pid = i;
            break;
        }
    }
    if (!proc) {
        spinlock_release(&proctbl_lock);
        return -EFULL;
    }

    loadprog_res_t res = load_program(path, argv, envp);
    if (res.status < 0) {
        kprint("failed to load program with code %d\n", res.status);
        spinlock_release(&proctbl_lock);
        return res.status;
    }

    struct stat st;
    int ret = 0;
    if ((ret = stat(path, &st)) < 0) {
        kprint("stat failed %d\n", ret);
        spinlock_release(&proctbl_lock);
        return ret;
    }

    if (pid == 0) {
        struct fdinfo* new_fds = malloc(sizeof(struct fdinfo) * 4);
        if (!new_fds) {
            kprint("no memory for fds\n");
            spinlock_release(&proctbl_lock);
            return -ENOMEM;
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
        if (!proc->pwd) {
            spinlock_release(&proctbl_lock);
            return -ENOMEM;
        }
        proc->pwd[0] = '/';
        proc->pwd[1] = '\0';
    } else {
        copy_fds(pid, ppid);
        proc->currfb = get_currfb();
        proc->ppid = ppid;
        proc->pwd = strdup(proctbl[proc->ppid].pwd);
        if (!proc->pwd) {
            spinlock_release(&proctbl_lock);
            return -ENOMEM;
        }
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
    proc->perms = 0;

    if (st.st_mode & S_ISUID) {
        proc->perms |= PROC_SUID;
    }

    if (st.st_mode & S_ISGID) {
        proc->perms |= PROC_SGID;
    }

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
    spinlock_release(&proctbl_lock);

    kprint("created new process %s with PID %d\n", path, proc->pid);
    return proc->pid;
}