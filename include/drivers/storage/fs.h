#pragma once
#include <core/std.h>
int fs_probe_mount(void); /* detect what's on the drive and mount it */

#define O_WRONLY 0x01
#define O_RDONLY 0x02
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT 0x04
#define O_APPEND 0x08
#define O_TRUNC 0x10

#define S_IFSOCK 0x400000
#define S_IFLNK  0x200000
#define S_IFREG  0x100000
#define S_IFBLK  0x080000
#define S_IFDIR  0x040000
#define S_IFCHR  0x020000
#define S_IFIFO  0x010000

#define S_ISUID  0x004000
#define S_ISGID  0x002000
#define S_ISVTX  0x001000

#define S_IRUSR  0x000400
#define S_IWUSR  0x000200
#define S_IXUSR  0x000100
#define S_IRGRP  0x000040
#define S_IWGRP  0x000020
#define S_IXGRP  0x000010
#define S_IROTH  0x000004
#define S_IWOTH  0x000002
#define S_IXOTH  0x000001

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s64 off_t;

struct stat {
    char st_name[256];
    u8 st_attrib;
    u32 mode;
    usize st_size;
} __attribute__((packed));

int mount(const char* path, int flags);
int umount(const char* path);

int open(const char* path, int flags, int mode);
int close(int fd);
ssize read(int fd, void* buf, usize size);
ssize write(int fd, void* buf, usize size);
off_t lseek(int fd, off_t off, int whence);
int trunc(int fd);
int sync(int fd);
int opendir(const char* path);
int closedir(int cdp);
int readdir(int cdp, struct stat* st);
int stat(const char* path, struct stat* st);
int unlink(const char* path);
int rename(const char* oname, const char* nname);
int mkdir(const char* path, int mode);
int chdir(const char* path);
int getcwd(char* buf, usize len);
int rmdir(const char* path);
int creat(const char* path, int mode);