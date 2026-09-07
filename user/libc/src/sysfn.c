#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/sysfn.h>

ssize read(int fd, void* buf, usize sz) {
    return (ssize)__syscall3(SYS_READ, (u64)fd, (u64)buf, (u64)sz);
}

ssize write(int fd, void* buf, usize sz) {
    return (ssize)__syscall3(SYS_WRITE, (u64)fd, (u64)buf, (u64)sz);
}

int reboot() {
    return (int)__syscall0(SYS_REBOOT);
}

int poweroff() {
    return (int)__syscall0(SYS_POWEROFF);
}

void sleep(int secs) {
    __syscall1(SYS_SLEEP, (u64)secs);
}

int ioctl(int fd, int cmd, void* data) {
    return (int)__syscall3(SYS_IOCTL, (u64)fd, (u64)cmd, (u64)data);
}

int termctl(int code, int arg) {
    return ioctl(STDOUT, code, (void*)(intptr_t)arg);
}