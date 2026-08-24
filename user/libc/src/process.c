#include <sys/process.h>
#include <sys/syscall.h>

int wait(int pid, int* code) {
    return (int)__syscall2(SYS_WAIT, (u64)(s64)pid, (u64)code);
}

int kill(int pid) {
    return (int)__syscall1(SYS_KILL, (u64)(s64)pid);
}

int newproc(const char* path, char** argv, char** envp) {
    return (int)__syscall3(SYS_NEWPROC, (u64)path, (u64)argv, (u64)envp);
}

int getpid() {
    return (int)__syscall0(SYS_GETPID);
}

uid_t getuid(void) {
    return (uid_t)__syscall0(SYS_GETUID);
}

int setuid(uid_t uid) {
    return (int)(s64)__syscall1(SYS_SETUID, (u64)uid);
}

gid_t getgid(void) {
    return (gid_t)__syscall0(SYS_GETGID);
}

int setgid(gid_t gid) {
    return (int)(s64)__syscall1(SYS_SETGID, (u64)gid);
}

uid_t geteuid(void) {
    return (uid_t)__syscall0(SYS_GETEUID);
}

int seteuid(uid_t euid) {
    return (int)(s64)__syscall1(SYS_SETEUID, (u64)euid);
}

gid_t getegid(void) {
    return (gid_t)__syscall0(SYS_GETEGID);
}

int setegid(gid_t egid) {
    return (int)(s64)__syscall1(SYS_SETEGID, (u64)egid);
}

int execve(char* path, char** argv, char** envp) {
    return (int)__syscall3(SYS_EXECVE, (u64)path, (u64)argv, (u64)envp);
}