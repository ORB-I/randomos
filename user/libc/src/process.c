#include <sys/process.h>
#include <sys/syscall.h>

int wait(int pid) {
    return (int)__syscall1(SYS_WAIT, (u64)(s64)pid);
}

int kill(int pid) {
    return (int)__syscall1(SYS_KILL, (u64)(s64)pid);
}

int newproc(const char* path, char** argv) {
    return (int)__syscall2(SYS_NEWPROC, (u64)path, (u64)argv);
}

int getpid() {
    return (int)__syscall0(SYS_GETPID);
}