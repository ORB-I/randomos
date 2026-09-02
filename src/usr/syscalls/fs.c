#include "../ensurance.h"
#include <core/std.h>
#include <drivers/storage/fs.h>
#include <lib/string.h>
#include <core/liballoc.h>
#include <scheduler/process.h>
#include "ssc.h"

DEFSYSCALL(sys_read) {
    if (!ensure_pointer((void*)args->a1, args->a2, 1)) return -EINVAL;
    return read(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_write) {
    if (!ensure_pointer((void*)args->a1, args->a2, 0)) return -EINVAL;
    return write(args->a0, (u8*)args->a1, args->a2);
}

DEFSYSCALL(sys_open) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return open((char*)args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_close) {
    return close(args->a0);
}

DEFSYSCALL(sys_creat) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return creat((char*)args->a0, args->a1);
}

DEFSYSCALL(sys_unlink) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return unlink((char*)args->a0);
}

DEFSYSCALL(sys_lseek) {
    return lseek(args->a0, args->a1, args->a2);
}

DEFSYSCALL(sys_rename) {
    if (!ensure_string((char*)args->a0, 256, 0) ||
        !ensure_string((char*)args->a1, 256, 0)) return -EINVAL;
    return rename((char*)args->a0, (char*)args->a1);
}

DEFSYSCALL(sys_mkdir) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return mkdir((char*)args->a0, (int)args->a1);
}

DEFSYSCALL(sys_rmdir) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return rmdir((char*)args->a0);
}

DEFSYSCALL(sys_stat) {
    if (!ensure_string((char*)args->a0, 256, 0) ||
        !ensure_pointer((void*)args->a1, sizeof(struct stat), 1)) return -EINVAL;
    return stat((char*)args->a0, (struct stat*)args->a1);
}

DEFSYSCALL(sys_readdir) {
    if (!ensure_pointer((void*)args->a1, sizeof(struct stat), 1)) return -EINVAL;
    return readdir((int)args->a0, (struct stat*)args->a1);
}

DEFSYSCALL(sys_opendir) {
    if (!ensure_string((char*)args->a0, 256, 0)) return -EINVAL;
    return (u64)opendir((char*)args->a0);
}

DEFSYSCALL(sys_sync) {
    return sync(args->a0);
}

DEFSYSCALL(sys_trunc) {
    return trunc(args->a0);
}

DEFSYSCALL(sys_getpwd) {
    if (!ensure_string((char*)args->a0, 256, 1)) return -EINVAL;
    char* buf = (char*)args->a0;
    usize bufsz = args->a1;

    char* pwd = proctbl[current_pid].pwd;
    if (!pwd || bufsz == 0) return -EINVAL;

    usize len = strlen(pwd) + 1;
    if (bufsz < len) {
        memcpy(buf, pwd, bufsz-1);
        buf[bufsz] = '\0';
    } else {
        memcpy(buf, pwd, len);
    }

    return 0;
}

#define PWD_PATH_MAX 1024

#include <drivers/display/serial.h>
DEFSYSCALL(sys_setpwd) {
    if (!ensure_string((char*)args->a0, 256, 0)) {
        serial_printf("invalid string\n");
        return -EINVAL;
    }

    char newpath[1025];
    if (canonicalize((char*)args->a0, newpath, 1025) < 0) {
        return -ETOOSMALL;
    }

    usize len = strlen(newpath) + 1;
    void* newpwd = malloc(len);
    if (!newpwd) {
        serial_printf("malloc failed\n");
        return -ENOMEM;
    }
    memcpy(newpwd, newpath, len);

    free(proctbl[current_pid].pwd);
    proctbl[current_pid].pwd = newpwd;
    return 0;
}