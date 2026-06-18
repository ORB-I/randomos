#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/sysfn.h>

ssize read(int fd, void* buf, usize sz) {
    return (ssize)__syscall3(SYS_READ, (u32)fd, (u32)buf, (u32)sz);
}

ssize write(int fd, void* buf, usize sz) {
    return (ssize)__syscall3(SYS_WRITE, (u32)fd, (u32)buf, (u32)sz);
}

int reboot() {
    return (int)__syscall0(SYS_REBOOT);
}

int poweroff() {
    return (int)__syscall0(SYS_POWEROFF);
}

void sleep(int secs) {
    __syscall1(SYS_SLEEP, (u32)secs);
}

int termctl(int code, int arg) {
    return (int)__syscall2(SYS_TERMCTL, (u32)code, (u32)arg);
}