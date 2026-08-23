#include <core/std.h>
#include <core/idt.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <core/liballoc.h>
#include <core/panic.h>
#include <core/mem/pmm.h>
#include <core/printf.h>

#include <drivers/time/gettimeofday.h>
#include <drivers/display/term.h>
#include <drivers/storage/fs.h>
#include <drivers/hid/mouse.h>
#include <drivers/display/fb.h>
#include <drivers/time/clock.h>
#include <drivers/hid/kbd.h>
#include <drivers/display/serial.h>

#include <lib/loader.h>
#include <lib/syscall.h>
#include <lib/string.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <lib/loader.h>

#include <lai/helpers/pm.h>

#define MSR_EFER          0xC0000080
#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_USER_GS_BASE 0xC0000101

extern void syscall_s();

void init_syscalls() {
    reset_kgsb();
    wrmsr(MSR_LSTAR, (u64)syscall_s);
    wrmsr(MSR_STAR, ((u64)0x1B << 48) | ((u64)0x08 << 32));
    // mask DF so user std can't make kernel memcpy/memset run backwards.
    // IF stays unmasked: the HPET clock timer must keep firing for sleepms()
    // to work, and the preempt timer sets a flag that syscall_s checks on
    // exit. the critical sections (scheduler_switch calls) are guarded with
    // explicit cli/sti instead.
    wrmsr(MSR_SFMASK, (1 << 10));
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);
}

struct sysregs {
    u64 num, a0, a1, a2, a3, a4, a5;
    u64 __es, __ds, __rflags, __rip;
};

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
        proctbl[pid].used = 0;
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

static s64 new_wait(s64 pid, int* code) {
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

// shoots another process dead from the outside, returns 0 when the pid
// is gone and -1 when its init, ourselves, doesnt exist or is already dead
static s64 kill_process(s64 pid) {
    if (pid <= 0 || pid >= MAX_PROCESSES || pid == current_pid ||
        !proctbl[pid].used || proctbl[pid].is_dead) {
        return -1;
    }

    /* Permission check: only root or owner can kill process */
    if (proctbl[current_pid].euid != 0 &&
        proctbl[current_pid].euid != proctbl[pid].uid &&
        proctbl[current_pid].uid != proctbl[pid].uid) {
        return -1;
    }

    // if it was parked in WAIT its never resuming, dont leave the
    // flag set behind
    proctbl[pid].is_blocked = 0;
    proctbl[pid].is_dead = 1;

    reparent_children((u8)pid);
    wake_waiter((u8)pid);

    // normal exits get their address space torn down by
    // scheduler_switch after the context switch, but nothing ever
    // switches away from this one so it has to happen here. that is
    // safe because were running on our own cr3, nobody can be inside
    // the targets page tables right now.
    vmm_dasp((page_table_t*)proctbl[pid].cr3);
    proctbl[pid].used = 0;
    return 0;
}

static int sys_newproc(page_table_t* uasp, const char *path, char **argv, u8 ppid) {
    // we need to copy our args into the kasp and switch
    // cuz if we dont new_process calls load_program
    // which without switching will overwrite the current user program

    usize nargs = 0;
    usize totalsz = sizeof(char*); // space for the NULL terminator
    while (nargs < 16 && argv[nargs] != NULL) { // 16 cuz thats ARGMAX
        totalsz += sizeof(char*) + strlen(argv[nargs]) + 1;
        nargs++;
    }
    totalsz += strlen(path) + 1;
    
    usize npgs = (totalsz + 4095) / 4096;

    void* phys = pmm_falloc(npgs);
    if (!phys) return -1;

    void* virt = (void*)((u64)phys + HHDM_START);
    char** kargv = (char**)virt;
    char* p = (char*)virt + sizeof(char*) * (nargs + 1);

    usize pathlen = strlen(path) + 1;

    char* kpath = p;

    memcpy(p, path, pathlen);
    p += pathlen;

    for (usize i = 0; i < nargs; i++) {
        usize len = strlen(argv[i]) + 1;
        kargv[i] = p;
        memcpy(p, argv[i], len);
        p += len;
    }

    kargv[nargs] = NULL;

    vmm_skasp();
    int ret = new_process(kpath, kargv, ppid);
    vmm_sasp(uasp);

    pmm_ffree(phys, npgs);

    return ret;
}

[[noreturn]] void sys_exit(int code) {
    vmm_skasp();
    proctbl[current_pid].code = code;
    proctbl[current_pid].is_dead = 1;
    reparent_children(current_pid);
    wake_waiter(current_pid);

    // the running context is being abandoned, so what we hand
    // to the scheduler as "saved state" doesn't matter — it only
    // gets archived into a process that will never run again.
    // cli prevents the preempt timer from nesting into
    // scheduler_switch and corrupting the context copy.
    asm volatile("cli");
    procctx_t abandoned = {0};
    scheduler_switch(&abandoned);

    // scheduler_switch only returns when nobody is left
    panic("all processes have exited");
}

bool syscall_c(struct sysregs* args) {
    page_table_t* uasp = vmm_cpml4v();
    struct sysregs svargs;
    memcpy(&svargs, args, sizeof(*args));

    switch (args->num) {
        case SYS_EXIT: {
            sys_exit(args->a0);
        }
        case SYS_READ: {
            if (!args->a1) { args->num = -1; goto ret; }
            args->num = read(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case SYS_WRITE: {
            if (!args->a1) { args->num = -1; goto ret; }
            args->num = write(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case SYS_OPEN: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = open((char*)args->a0, args->a1);
            goto ret;
        }
        case SYS_CLOSE: {
            args->num = close(args->a0);
            goto ret;
        }
        case SYS_CREAT: {
            if (!args->a0) { args->num = -1; goto ret; }
            int fd;
            if ((fd = open((char*)args->a0, O_CREAT | O_WRONLY | O_TRUNC)) < 0) {
                args->num = -1;
                goto ret;
            } else {
                args->num = close(fd);
                goto ret;
            }
        }
        case SYS_UNLINK: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case SYS_CHDIR: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = chdir((char*)args->a0);
            goto ret;
        }
        case SYS_LSEEK: {
            args->num = lseek(args->a0, args->a1, args->a2);
            goto ret;
        }
        case SYS_RENAME: {
            if (!args->a0 || !args->a1) { args->num = -1; goto ret; }
            args->num = rename((char*)args->a0, (char*)args->a1);
            goto ret;
        }
        case SYS_MKDIR: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = mkdir((char*)args->a0);
            goto ret;
        }
        case SYS_RMDIR: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case SYS_REBOOT: {
            if (lai_acpi_reset() == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case SYS_STAT: {
            if (!args->a0 || !args->a1) { args->num = -1; goto ret; }
            args->num = stat((char*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case SYS_POWEROFF: {
            if (lai_enter_sleep(5) == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case SYS_SLEEP: {
            if (args->a0 == 0) {
                preempt_pending = 1;
            } else {
                proctbl[current_pid].wake_ms = (getms ? getms() : 0) + args->a0;
                proctbl[current_pid].is_blocked = 1;
                preempt_pending = 1;
            }
            args->num = 0;
            goto ret;
        }
        case SYS_READDIR: {
            if (!args->a0 || !args->a1) { args->num = -1; goto ret; }
            args->num = readdir((DIR*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case SYS_OPENDIR: {
            if (!args->a0) { args->num = 0; goto ret; }
            args->num = (u64)opendir((char*)args->a0);
            goto ret;
        }
        case SYS_CLOSEDIR: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = closedir((DIR*)args->a0);
            goto ret;
        }
        case SYS_GETCWD: {
            if (!args->a0) { args->num = -1; goto ret; }
            args->num = getcwd((char*)args->a0, args->a1);
            goto ret;
        }
        case SYS_SYNC: {
            args->num = sync(args->a0);
            goto ret;
        }
        case SYS_TRUNC: {
            args->num = trunc(args->a0);
            goto ret;
        }
        case SYS_TERMCTL: {
            args->num = termctl(args->a0, args->a1);
            goto ret;
        }
        case SYS_CREATEFB: {
            args->num = create_fb(args->a0);
            goto ret;
        }
        case SYS_RMFB: {
            free_fb(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_SWITCHFB: {
            args->num = switch_fb(args->a0);
            goto ret;
        }
        case SYS_CLEARFB: {
            clear_fb(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_FLUSHSCR: {
            flush_scr();
            args->num = 0;
            goto ret;
        }
        case SYS_GETFBINF: {
            if (!args->a1) { args->num = -1; goto ret; }
            args->num = get_fbinfo(args->a0, (framebuf_info_t*)args->a1);
            goto ret;
        }
        case SYS_GETFBTYP: {
            args->num = get_typefb(args->a0);
            goto ret;
        }
        case SYS_GETCURFB: {
            args->num = get_currfb();
            goto ret;
        }
        case SYS_GETTIMEOFDAY: {
            args->num = gettimeofday();
            goto ret;
        }
        case SYS_GETMTIMEOFDAY: {
            if (!args->a0) { args->num = -1; goto ret; }
            getmtimeofday((struct millitime*)args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_GETTIMEMONOMS: {
            args->num = getms();
            goto ret;
        }
        case SYS_GETTIMEMONO: {
            args->num = rdtsc();
            goto ret;
        }
        case SYS_MMAP: {
            args->num = (u64)user_mmap(uasp, (void*)args->a0, args->a1);
            goto ret;
        }
        case SYS_MUNMAP: {
            args->num = user_munmap(uasp, (void*)args->a0, args->a1);
            goto ret;
        }
        case SYS_GETRAWSC: {
            args->num = kbd_get_raw();
            goto ret;
        }
        case SYS_CREATEFBWMEM: {
            args->num = create_fb_withmem(args->a0, (void*)args->a1, args->a2, (int*)args->a3);
            goto ret;
        }
        case SYS_RMFBWMEM: {
            free_fb_withmem(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_GETMOUSEINFO: {
            if (!args->a0) { 
                args->num = -1; goto ret; 
            }
            args->num = get_mouse_info((mouse_info_t*)args->a0);
            goto ret;
        }
        case SYS_NEWPROC: {
            if (!args->a0 || !args->a1) { args->num = -1; goto ret; }
            args->num = sys_newproc(uasp, (char*)args->a0, (char**)args->a1, current_pid);
            goto ret;
        }
        case SYS_WAIT: {
            args->num = new_wait((s64)args->a0, (int*)args->a1);
            goto ret;
        }
        case SYS_KILL: {
            args->num = kill_process((s64)args->a0);
            goto ret;
        }
        case SYS_GETPID: {
            args->num = current_pid;
            goto ret;
        }
        case SYS_GETUID: {
            args->num = (u64)proctbl[current_pid].uid;
            goto ret;
        }
        case SYS_SETUID: {
            uid_t nuid = (uid_t)args->a0;
            if (proctbl[current_pid].euid == 0) {
                proctbl[current_pid].uid = nuid;
                proctbl[current_pid].euid = nuid;
                args->num = 0;
            } else if (nuid == proctbl[current_pid].uid || nuid == proctbl[current_pid].euid) {
                proctbl[current_pid].euid = nuid;
                args->num = 0;
            } else {
                args->num = (u64)(s64)-1;
            }
            goto ret;
        }
        case SYS_GETGID: {
            args->num = (u64)proctbl[current_pid].gid;
            goto ret;
        }
        case SYS_SETGID: {
            gid_t ngid = (gid_t)args->a0;
            if (proctbl[current_pid].euid == 0) {
                proctbl[current_pid].gid = ngid;
                proctbl[current_pid].egid = ngid;
                args->num = 0;
            } else if (ngid == proctbl[current_pid].gid || ngid == proctbl[current_pid].egid) {
                proctbl[current_pid].egid = ngid;
                args->num = 0;
            } else {
                args->num = (u64)(s64)-1;
            }
            goto ret;
        }
        case SYS_GETEUID: {
            args->num = (u64)proctbl[current_pid].euid;
            goto ret;
        }
        case SYS_SETEUID: {
            uid_t neuid = (uid_t)args->a0;
            if (proctbl[current_pid].euid == 0 ||
                neuid == proctbl[current_pid].uid ||
                neuid == proctbl[current_pid].euid) {
                proctbl[current_pid].euid = neuid;
                args->num = 0;
            } else {
                args->num = (u64)(s64)-1;
            }
            goto ret;
        }
        case SYS_GETEGID: {
            args->num = (u64)proctbl[current_pid].egid;
            goto ret;
        }
        case SYS_SETEGID: {
            gid_t negid = (gid_t)args->a0;
            if (proctbl[current_pid].euid == 0 ||
                negid == proctbl[current_pid].gid ||
                negid == proctbl[current_pid].egid) {
                proctbl[current_pid].egid = negid;
                args->num = 0;
            } else {
                args->num = (u64)(s64)-1;
            }
            goto ret;
        }
        case SYS_SERIALWRITE: {
                for (usize i = 0; i < args->a1; i++) {
                    serial_putchar(((char*)args->a0)[i]);
                }
                args->num = 0;
                goto ret;
            }
            default: args->num = -1;
        }
    ret: {
        vmm_sasp(uasp); // idk why just try
        u64 ret = args->num;
        memcpy(args, &svargs, sizeof(*args));
        args->num = ret;
    }
    return false;

}