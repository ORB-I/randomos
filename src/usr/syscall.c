#include "core/errno.h"
#include <core/std.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <lib/loader.h>
#include <lib/syscall.h>
#include <lib/string.h>

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

#include "syscalls/ssc.h"
#include "syscalls/fs.h"
#include "syscalls/process.h"
#include "syscalls/system.h"
#include "syscalls/time.h"
#include "syscalls/io.h"

syscall_hdlr_t syscall_tbl[] = {
    [SYS_EXIT] = sys_exit,
    [SYS_READ] = sys_read,
    [SYS_WRITE] = sys_write,
    [SYS_OPEN] = sys_open,
    [SYS_CLOSE] = sys_close,
    [SYS_CREAT] = sys_creat,
    [SYS_UNLINK] = sys_unlink,
    [SYS_LSEEK] = sys_lseek,
    [SYS_RENAME] = sys_rename,
    [SYS_MKDIR] = sys_mkdir,
    [SYS_RMDIR] = sys_rmdir,
    [SYS_REBOOT] = sys_reboot,
    [SYS_STAT] = sys_stat,
    [SYS_POWEROFF] = sys_poweroff,
    [SYS_SLEEP] = sys_sleep,
    [SYS_READDIR] = sys_readdir,
    [SYS_OPENDIR] = sys_opendir,
    [SYS_SYNC] = sys_sync,
    [SYS_TERMCTL] = sys_termctl,
    [SYS_CREATEFB] = sys_createfb,
    [SYS_SWITCHFB] = sys_switchfb,
    [SYS_CLEARFB] = sys_clearfb,
    [SYS_FLUSHSCR] = sys_flushscr,
    [SYS_GETFBINF] = sys_getfbinf,
    [SYS_GETCURFB] = sys_getcurfb,
    [SYS_GETTIMEOFDAY] = sys_gettimeofday,
    [SYS_GETMTIMEOFDAY] = sys_getmtimeofday,
    [SYS_GETTIMEMONOMS] = sys_gettimemonoms,
    [SYS_GETTIMEMONO] = sys_gettimemono,
    [SYS_MMAP] = sys_mmap,
    [SYS_MUNMAP] = sys_munmap,
    [SYS_GETRAWSC] = sys_getrawsc,
    [SYS_GETMOUSEINFO] = sys_getmouseinfo,
    [SYS_NEWPROC] = sys_newproc,
    [SYS_WAIT] = sys_wait,
    [SYS_KILL] = sys_kill,
    [SYS_GETPID] = sys_getpid,
    [SYS_GETUID] = sys_getuid,
    [SYS_SETUID] = sys_setuid,
    [SYS_GETGID] = sys_getgid,
    [SYS_SETGID] = sys_setgid,
    [SYS_GETEUID] = sys_geteuid,
    [SYS_SETEUID] = sys_geteuid,
    [SYS_GETEGID] = sys_getegid,
    [SYS_SETEGID] = sys_setegid,
    [SYS_SERIALWRITE] = sys_serialwrite,
    [SYS_GETRAWSCTO] = sys_getrawscto,
    [SYS_EXECVE] = sys_execve,
    [SYS_RANDOM64] = sys_random64,
    [SYS_RANDOMBYTES] = sys_randombytes,
    [SYS_GETPWD] = sys_getpwd,
    [SYS_SETPWD] = sys_setpwd,
    [SYS_SETCURS] = sys_setcurs,
    [SYS_GETCURS] = sys_getcurs,
    [SYS_MPROTECT] = sys_mprotect
};

bool syscall_c(struct sysregs* args) {
    page_table_t* uasp = vmm_cpml4v();
    struct sysregs svargs;
    memcpy(&svargs, args, sizeof(*args));

    if (args->num > (sizeof(syscall_tbl)/sizeof(syscall_hdlr_t)) ||
        !syscall_tbl[args->num]) return -EINVAL;
    
    u64 ret = syscall_tbl[args->num](args);
    
    vmm_sasp(uasp);
    memcpy(args, &svargs, sizeof(*args));
    args->num = ret;
    return 0;
}