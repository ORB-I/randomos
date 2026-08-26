#include "../ensurance.h"
#include <core/std.h>
#include <drivers/storage/fs.h>
#include <lib/string.h>
#include <core/liballoc.h>
#include <scheduler/process.h>
#include "ssc.h"

DEFSYSCALL(sys_read) {
    if (!ensure_pointer((void*)args->a1, args->a2, 1)) return -1;
    return read(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_write) {
    if (!ensure_pointer((void*)args->a1, args->a2, 0)) return -1;
    return write(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_open) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;
    return open((char*)args->a0, args->a1);
}

DEFSYSCALL(sys_close) {
    return close(args->a0);
}

DEFSYSCALL(sys_creat) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;
    int fd;
    if ((fd = open((char*)args->a0, O_CREAT | O_WRONLY | O_TRUNC)) < 0) {
        return -1;
    } else {
        return close(fd);
    }
}

DEFSYSCALL(sys_unlink) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;
    return unlink((char*)args->a0);
}

DEFSYSCALL(sys_lseek) {
    return lseek(args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_rename) {
    if (!ensure_string((char*)args->a0, 256, 0) ||
        !ensure_string((char*)args->a1, 256, 0)) return -1;
    return rename((char*)args->a0, (char*)args->a1);
}

DEFSYSCALL(sys_mkdir) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;
    return mkdir((char*)args->a0);
}

DEFSYSCALL(sys_rmdir) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;
    return unlink((char*)args->a0);
}

DEFSYSCALL(sys_stat) {
    if (!ensure_string((char*)args->a0, 256, 0) ||
        !ensure_pointer((void*)args->a1, sizeof(struct stat), 1)) return -1;
    return stat((char*)args->a0, (struct stat*)args->a1);
}

DEFSYSCALL(sys_readdir) {
    if (!ensure_pointer((void*)args->a1, sizeof(struct stat), 1)) {
        return -1;
    }
    return readdir((int)args->a0, (struct stat*)args->a1);
}

DEFSYSCALL(sys_opendir) {
    if (!ensure_string((char*)args->a0, 256, 0)) {
        return -1;
    }
    return (u64)opendir((char*)args->a0);
}

DEFSYSCALL(sys_sync) {
    return sync(args->a0);
}

DEFSYSCALL(sys_trunc) {
    return trunc(args->a0);
}

DEFSYSCALL(sys_getpwd) {
    if (!ensure_string((char*)args->a0, 256, 1)) return -1;
    char* buf = (char*)args->a0;
    usize bufsz = args->a1;

    char* pwd = proctbl[current_pid].pwd;
    if (!pwd || bufsz == 0) return -1;

    /* bound the copy by the string, not the caller's buffer: a short
       pwd must not drag in whatever sits past it in kernel memory */
    usize len = strlen(pwd) + 1;
    if (bufsz < len) return -1;
    memcpy(buf, pwd, len);

    return 0;
}

#define PWD_PATH_MAX 256

DEFSYSCALL(sys_setpwd) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -1;

    char canon[PWD_PATH_MAX];
    if (chdir((char*)args->a0) != 0) return -1;
    if (getcwd(canon, sizeof(canon)) != 0) return -1;

    usize len = strlen(canon) + 1;
    void* newpwd = malloc(len);
    if (!newpwd) return -1;
    memcpy(newpwd, canon, len);

    free(proctbl[current_pid].pwd);
    proctbl[current_pid].pwd = newpwd;
    return 0;
}