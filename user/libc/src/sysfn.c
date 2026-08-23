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

int termctl(int code, int arg) {
    return (int)__syscall2(SYS_TERMCTL, (u64)code, (u64)arg);
}

int wait(int pid) {
    return (int)__syscall1(SYS_WAIT, (u64)(s64)pid);
}

int kill(int pid) {
    return (int)__syscall1(SYS_KILL, (u64)(s64)pid);
}

int newproc(const char* path, char** argv) {
    return (int)__syscall2(SYS_NEWPROC, (u64)path, (u64)argv);
}