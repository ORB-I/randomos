#pragma once
#include <sys/types.h>

#define O_WRONLY 0x02
#define O_RDONLY 0x01
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT 0x10
#define O_APPEND 0x30
#define O_TRUNC 0x04

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s32 off_t;

struct stat {
    char st_name[256];
    u8 st_attrib;
    u32 mode;
    usize st_size;
} __attribute__((packed));

int creat(char* path);
int unlink(char* path);
int chdir(char* path);
off_t lseek(int fd, off_t off, u32 whence);
int rename(char* oldname, char* newname);
int mkdir(char* path);
int rmdir(char* path);
int stat(char* path, struct stat* st);
int readdir(int dir, struct stat* st);
int opendir(char* path);
int getcwd(char* buf, usize buflen);
int sync(int fd);
int trunc(int fd);