#include <fs.h>
#include <sys/syscall.h>

int open(char* path, u32 flags) {
    return (int)__syscall2(SYS_OPEN, (u32)path, flags);
}

int close(int fd) {
    return (int)__syscall1(SYS_CLOSE, (u32)fd);
}

int creat(char* path) {
    return (int)__syscall1(SYS_CREAT, (u32)path);
}

int unlink(char* path) {
    return (int)__syscall1(SYS_UNLINK, (u32)path);
}

int chdir(char* path) {
    return (int)__syscall1(SYS_CHDIR, (u32)path);
}

off_t lseek(int fd, off_t off, u32 whence) {
    return (off_t)__syscall3(SYS_LSEEK, (u32)fd, (u32)off, whence);
}

int rename(char* oldname, char* newname) {
    return (int)__syscall2(SYS_RENAME, (u32)oldname, (u32)newname);
}

int mkdir(char* path) {
    return (int)__syscall1(SYS_MKDIR, (u32)path);
}

int rmdir(char* path) {
    return (int)__syscall1(SYS_RMDIR, (u32)path);
}

int stat(char* path, struct stat* st) {
    return (int)__syscall2(SYS_STAT, (u32)path, (u32)st);
}

int readdir(DIR* dir, struct stat* st) {
    return (int)__syscall2(SYS_READDIR, (u32)dir, (u32)st);
}

DIR* opendir(char* path) {
    return (DIR*)__syscall1(SYS_OPENDIR, (u32)path);
}

int closedir(DIR* dir) {
    return (int)__syscall1(SYS_CLOSEDIR, (u32)dir);
}

int getcwd(char* buf, usize buflen) {
    return (int)__syscall2(SYS_GETCWD, (u32)buf, (u32)buflen);
}

int sync(int fd) {
    return (int)__syscall1(SYS_SYNC, (u32)fd);
}

int trunc(int fd) {
    return (int)__syscall1(SYS_TRUNC, (u32)fd);
}