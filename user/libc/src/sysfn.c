#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/sysfn.h>

ssize read(int fd, void* buf, usize sz) {
    return (ssize)syscall(SYS_READ, (u32)fd, (u32)buf, (u32)sz);
}

ssize write(int fd, void* buf, usize sz) {
    return (ssize)syscall(SYS_WRITE, (u32)fd, (u32)buf, (u32)sz);
}

int open(char* path, u32 flags) {
    return (int)syscall(SYS_OPEN, (u32)path, flags);
}

int close(int fd) {
    return (int)syscall(SYS_CLOSE, (u32)fd);
}

int creat(char* path) {
    return (int)syscall(SYS_CREAT, (u32)path);
}

int unlink(char* path) {
    return (int)syscall(SYS_UNLINK, (u32)path);
}

int chdir(char* path) {
    return (int)syscall(SYS_CHDIR, (u32)path);
}

off_t lseek(int fd, off_t off, u32 whence) {
    return (off_t)syscall(SYS_LSEEK, (u32)fd, (u32)off, whence);
}

int rename(char* oldname, char* newname) {
    return (int)syscall(SYS_RENAME, (u32)oldname, (u32)newname);
}

int mkdir(char* path) {
    return (int)syscall(SYS_MKDIR, (u32)path);
}

int rmdir(char* path) {
    return (int)syscall(SYS_RMDIR, (u32)path);
}

int reboot() {
    return (int)syscall(SYS_REBOOT);
}

int stat(char* path, struct stat* st) {
    return (int)syscall(SYS_STAT, (u32)path, (u32)st);
}

int poweroff() {
    return (int)syscall(SYS_POWEROFF);
}

void sleep(int secs) {
    syscall(SYS_SLEEP, (u32)secs);
}