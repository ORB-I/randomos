#pragma once
#include <core/std.h>
#include <ff16/ff.h>

#define MNT_FORMAT 0x01

/* which filesystem owns the mounted drive */
#define FS_BACKEND_FAT  0
#define FS_BACKEND_EXT2 1
extern int fs_backend;

int fs_probe_mount(void); /* detect what's on the drive and mount it */

#define O_WRONLY FA_WRITE
#define O_RDONLY FA_READ
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT FA_OPEN_ALWAYS
#define O_APPEND FA_OPEN_APPEND
#define O_TRUNC 0x04

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s32 off_t;

struct stat {
    char st_name[256];
    u8 st_attrib;
    usize st_size;
} __attribute__((packed));

int mount(const char* path, int flags);
int umount(const char* path);

int open(const char* path, int flags);
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
int mkdir(const char* path);
int chdir(const char* path);
int getcwd(char* buf, usize len);