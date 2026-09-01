#include <core/std.h>
#include <core/printf.h>
#include <core/errno.h>
#include <lib/string.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/ext2.h>
#include <drivers/display/serial.h>
#include <drivers/display/term.h>
#include <drivers/hid/kbd.h>
#include <core/fd.h>

int fs_probe_mount(void) {
    if (_ext2_mount("/") < 0) {
        printf("Failed to mount EXT2\n");
        return -ENOEXIST;
    }
    return 0;
}

int umount(const char* path) {
    (void)path;
    return _ext2_unmount();
}

int open(const char* path, int flags, int mode) {
    if (flags & O_CREAT) {
        mode |= S_IFREG;
    }

    return _ext2_open(path, flags, mode);
}

off_t lseek(int fd, off_t off, int whence) {
    return _ext2_lseek(fd, off, whence);
}

int trunc(int fd) {
    return _ext2_trunc(fd);
}

int sync(int fd) {
    return _ext2_sync(fd);
}

int opendir(const char* path) {
    return _ext2_opendir(path);
}

int closedir(int cdp) {
    return _ext2_close(cdp);
}

int readdir(int cdp, struct stat* st) {
    return _ext2_readdir(cdp, st);
}

int stat(const char* path, struct stat* st) {
    return _ext2_stat(path, st);
}

int unlink(const char* path) {
    return _ext2_unlink(path);
}

int rmdir(const char* path) {
    return _ext2_rmdir(path);
}

int rename(const char* oname, const char* nname) {
    return _ext2_rename(oname, nname);
}

int mkdir(const char* path, int mode) {
    return _ext2_mkdir(path, mode);
}

int chdir(const char* path) {
    return _ext2_chdir(path);
}

int getcwd(char* buf, usize len) {
    return _ext2_getcwd(buf, len);
}

int creat(const char* path, int mode) {
    return _ext2_creat(path, mode | S_IFREG);
}

int canonicalize(const char* path, char* out, usize outlen) {
    return _ext2_canon(path, out, outlen);
}