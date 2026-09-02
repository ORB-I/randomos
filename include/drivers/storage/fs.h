#pragma once
#include <core/std.h>
#include <drivers/storage/fs/vfs.h>
int fs_probe_mount(void); /* detect what's on the drive and mount it */

#define O_WRONLY 0x01
#define O_RDONLY 0x02
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT 0x04
#define O_APPEND 0x08
#define O_TRUNC 0x10

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s64 off_t;

struct stat {
    char st_name[1024];
    u16 st_uid;
    u32 st_atime;
    u32 st_ctime;
    u32 st_mtime;
    u16 st_gid;
    u16 st_mode;
    usize st_size;
    u64 st_ino;
} __attribute__((packed));

int mount(const char* dev, const char* path, const char* type);
int umount(const char* path);

int open(const char* path, int flags, u16 mode);
int close(int fd);
ssize read(int fd, void* buf, usize size);
ssize write(int fd, void* buf, usize size);
off_t lseek(int fd, off_t off, int whence);
int trunc(int fd);
int sync(int fd);
int opendir(const char* path);
int closedir(int cp);
int readdir(int dd, struct stat* st);
int stat(const char* path, struct stat* st);
int unlink(const char* path);
int rename(const char* oname, const char* nname);
int mkdir(const char* path, int mode);
int chdir(const char* path);
int getcwd(char* buf, usize len);
int rmdir(const char* path);
int creat(const char* path, int mode);
int canonicalize(const char* path, char* out, usize outlen);
ssize fsread(int fd, void* buf, usize sz);
ssize fswrite(int fd, void* buf, usize sz);