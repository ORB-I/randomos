#include <fs.h>
#include <sys/syscall.h>

int open(char* path, u32 flags) {
    return (int)__syscall2(SYS_OPEN, (u64)path, flags);
}

int close(int fd) {
    return (int)__syscall1(SYS_CLOSE, (u32)fd);
}

int creat(char* path) {
    return (int)__syscall1(SYS_CREAT, (u64)path);
}

int unlink(char* path) {
    return (int)__syscall1(SYS_UNLINK, (u64)path);
}

int chdir(char* path) {
    return (int)__syscall1(SYS_CHDIR, (u64)path);
}

off_t lseek(int fd, off_t off, u32 whence) {
    return (off_t)__syscall3(SYS_LSEEK, (u32)fd, (u32)off, whence);
}

int rename(char* oldname, char* newname) {
    return (int)__syscall2(SYS_RENAME, (u64)oldname, (u64)newname);
}

int mkdir(char* path) {
    return (int)__syscall1(SYS_MKDIR, (u64)path);
}

int rmdir(char* path) {
    return (int)__syscall1(SYS_RMDIR, (u64)path);
}

int stat(char* path, struct stat* st) {
    return (int)__syscall2(SYS_STAT, (u64)path, (u64)st);
}

int readdir(DIR* dir, struct stat* st) {
    return (int)__syscall2(SYS_READDIR, (u64)dir, (u64)st);
}

DIR* opendir(char* path) {
    return (DIR*)__syscall1(SYS_OPENDIR, (u64)path);
}

int closedir(DIR* dir) {
    return (int)__syscall1(SYS_CLOSEDIR, (u64)dir);
}

int getcwd(char* buf, usize buflen) {
    return (int)__syscall2(SYS_GETCWD, (u64)buf, (u64)buflen);
}

int sync(int fd) {
    return (int)__syscall1(SYS_SYNC, (u32)fd);
}

int trunc(int fd) {
    return (int)__syscall1(SYS_TRUNC, (u32)fd);
}