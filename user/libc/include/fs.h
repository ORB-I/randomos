#pragma once
#include <sys/types.h>

#define O_WRONLY 0x02
#define O_RDONLY 0x01
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT 0x10
#define O_APPEND 0x30
#define O_TRUNC 0x80

#define S_IFSOCK 0xC000
#define S_IFLNK  0xA000
#define S_IFREG  0x8000
#define S_IFBLK  0x6000
#define S_IFDIR  0x4000
#define S_IFCHR  0x2000
#define S_IFIFO  0x1000
#define S_TYPE(MODE) (MODE & 0xF000)

#define S_ISUID 0x0800
#define S_ISGID 0x0400
#define S_ISVTX 0x0200

#define S_IRUSR 0x0100
#define S_IWUSR 0x0080
#define S_IXUSR 0x0040
#define S_IRGRP 0x0020
#define S_IWGRP 0x0010
#define S_IXGRP 0x0008
#define S_IROTH 0x0004
#define S_IWOTH 0x0002
#define S_IXOTH 0x0001

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s64 off_t;

struct __attribute__((packed)) stat {
    char st_name[1024];
    u16 st_uid;
    u32 st_atime;
    u32 st_ctime;
    u32 st_mtime;
    u16 st_gid;
    u16 st_mode;
    usize st_size;
    u64 st_ino;
};

int creat(char* path, int mode);
int unlink(char* path);
int chdir(char* path);
off_t lseek(int fd, off_t off, u32 whence);
int rename(char* oldname, char* newname);
int mkdir(char* path, u32 mode);
int rmdir(char* path);
int stat(char* path, struct stat* st);
int readdir(int dir, struct stat* st);
int opendir(char* path);
int getcwd(char* buf, usize buflen);
int sync(int fd);
int trunc(int fd);