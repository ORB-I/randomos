#include "../ssc.h"
#include <scheduler/process.h>
#include <core/errno.h>

DEFSYSCALL(sys_getpid) {
    (void)args;
    return current_pid;
}

DEFSYSCALL(sys_getuid) {
    (void)args;
    return (u64)proctbl[current_pid].uid;
}

// should check fsperms
// to see if this program is setuid/setgid
DEFSYSCALL(sys_setuid) {
    uid_t nuid = (uid_t)args->a0;
    if (proctbl[current_pid].perms & PROC_SUID || 
        proctbl[current_pid].euid == 0 ||
        nuid == proctbl[current_pid].uid || 
        nuid == proctbl[current_pid].euid) {
            proctbl[current_pid].uid = nuid;
            return 0;
    }  else {
        return -EACCESS;
    }
}

DEFSYSCALL(sys_getgid) {
    (void)args;
    return (u64)proctbl[current_pid].gid;
}

DEFSYSCALL(sys_setgid) {
    gid_t ngid = (gid_t)args->a0;
    if (proctbl[current_pid].perms & PROC_SGID ||
        proctbl[current_pid].euid == 0 ||
        ngid == proctbl[current_pid].gid || 
        ngid == proctbl[current_pid].egid) {
            proctbl[current_pid].gid = ngid;
            return 0;
    }  else {
        return -EACCESS;
    }
}

DEFSYSCALL(sys_geteuid) {
    (void)args;
    return (u64)proctbl[current_pid].euid;
}

DEFSYSCALL(sys_seteuid) {
    uid_t neuid = (uid_t)args->a0;
    if (proctbl[current_pid].perms & PROC_SUID ||
        proctbl[current_pid].euid == 0 ||
        neuid == proctbl[current_pid].uid ||
        neuid == proctbl[current_pid].euid) {
            proctbl[current_pid].euid = neuid;
            return 0;
    } else {
        return -EACCESS;
    }
}

DEFSYSCALL(sys_getegid) {
    (void)args;
    return (u64)proctbl[current_pid].egid;
}

DEFSYSCALL(sys_setegid) {
    gid_t negid = (gid_t)args->a0;
    if (proctbl[current_pid].perms & PROC_SGID ||
        proctbl[current_pid].euid == 0 ||
        negid == proctbl[current_pid].gid ||
        negid == proctbl[current_pid].egid) {
            proctbl[current_pid].egid = negid;
            return 0;
    } else {
        return -EACCESS;
    }
}