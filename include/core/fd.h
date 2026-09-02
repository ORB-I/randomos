#pragma once
#include <core/std.h>
#include <drivers/display/fb.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/fs/vfs.h>

#define FDTYPE_FILE  1
#define FDTYPE_DIR   2
#define FDTYPE_SOCK  3
#define FDTYPE_FB    4
#define FDTYPE_FBW   5
#define FDTYPE_IO    6 // for stdin,stdout,stderr
#define FDTYPE_DEV   7

struct iofd {
    int in;
    int out;
    ssize (*write)(void* buf, usize str);
    ssize (*read)(void* buf, usize str);
};

struct file {
    vfs_t* mnt;
    vinode_t inod;
    u64 ino;
    usize pos;
    usize mnt_pos;
    char path[1024];
};

struct device {
    u16 mode;
    u16 rdev;
};

struct fdinfo {
    int fd;
    int inuse;
    int type;
    union {
        struct file file;
        struct file dir;
        int sock;
        framebuf_t* fb;
        struct iofd io;
        struct device dev;
    } data;
};

struct fdinfo* getnewfd(struct fdinfo* info);
int getfd(int fd, struct fdinfo** info);
int closefd(int fd);
int close(int fd);