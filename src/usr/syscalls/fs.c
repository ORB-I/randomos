#include <core/std.h>
#include <drivers/storage/fs.h>
#include <lib/string.h>
#include <core/liballoc.h>
#include <scheduler/process.h>
#include "ssc.h"

DEFSYSCALL(sys_read) {
    if (!args->a1) { return -1; }
    return read(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_write) {
    if (!args->a1) { return -1; }
    return write(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_open) {
    if (!args->a0) { return -1; }
    return open((char*)args->a0, args->a1);
}

DEFSYSCALL(sys_close) {
    return close(args->a0);
}

DEFSYSCALL(sys_creat) {
    if (!args->a0) { return -1; }
    int fd;
    if ((fd = open((char*)args->a0, O_CREAT | O_WRONLY | O_TRUNC)) < 0) {
        return -1;
    } else {
        return close(fd);
    }
}

DEFSYSCALL(sys_unlink) {
    if (!args->a0) { return -1; }
    return unlink((char*)args->a0);
}

DEFSYSCALL(sys_lseek) {
    return lseek(args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_rename) {
    if (!args->a0 || !args->a1) return -1;
    return rename((char*)args->a0, (char*)args->a1);
}

DEFSYSCALL(sys_mkdir) {
    if (!args->a0) return -1;
    return mkdir((char*)args->a0);
}

DEFSYSCALL(sys_rmdir) {
    if (!args->a0) return -1;
    return unlink((char*)args->a0);
}

DEFSYSCALL(sys_stat) {
    if (!args->a0 || !args->a1) return -1;
    return stat((char*)args->a0, (struct stat*)args->a1);
}

//NEW

DEFSYSCALL(sys_readdir) {
    if (!args->a0 || !args->a1) return -1;
    return readdir((int)args->a0, (struct stat*)args->a1);
}

DEFSYSCALL(sys_opendir) {
    if (!args->a0) return -1;
    return (u64)opendir((char*)args->a0);
}

DEFSYSCALL(sys_sync) {
    return sync(args->a0);
}

DEFSYSCALL(sys_trunc) {
    return trunc(args->a0);
}

DEFSYSCALL(sys_getpwd) {
    if (!args->a0) return -1;
    char* buf = (char*)args->a0;
    usize bufsz = args->a1;

    memcpy(buf, proctbl[current_pid].pwd, bufsz-1);
    buf[bufsz-1] = '\0';

    return 0;
}

DEFSYSCALL(sys_setpwd) {
    if (!args->a0) return -1;
    usize len = strlen((char*)args->a0) + 1;
    void* newpwd = malloc(len);
    if (!newpwd) return -1;
    free(proctbl[current_pid].pwd);
    proctbl[current_pid].pwd = newpwd;
    memcpy(proctbl[current_pid].pwd, (char*)args->a0, len);
    return 0;
}