#pragma once
#include <ff16/ff.h>
#include <core/std.h>
#include <drivers/display/fb.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/ext2.h>

#define FDTYPE_FILE  1
#define FDTYPE_DIR   2
#define FDTYPE_SOCK  3
#define FDTYPE_FB    4
#define FDTYPE_FBW   5
#define FDTYPE_IO    6 // for stdin,stdout,stderr
#define FDTYPE_E2ENT 7

struct iofd {
    int in;
    int out;
    ssize (*write)(void* buf, usize str);
    ssize (*read)(void* buf, usize str);
};

struct fdinfo {
    int fd;
    int inuse;
    int type;
    union {
        FIL file;
        DIR dir;
        int sock;
        framebuf_t* fb;
        struct iofd io;
        struct ext2_entry e2ent;
    } data;
};

struct fdinfo* getnewfd(struct fdinfo* info);
int getfd(int fd, struct fdinfo** info);
int closefd(int fd);
int close(int fd);