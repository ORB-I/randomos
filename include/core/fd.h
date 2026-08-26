#pragma once
#include <ff16/ff.h>
#include <core/std.h>
#include <drivers/display/fb.h>
#include <drivers/storage/fs.h>

#define FDTYPE_FILE 1
#define FDTYPE_DIR  2
#define FDTYPE_SOCK 3
#define FDTYPE_FB   4
#define FDTYPE_FBW  5
#define FDTYPE_IO   6 // for stdin,stdout,stderr

struct iofd {
    int in;
    int out;
    ssize (*write)(void* buf, usize str);
    ssize (*read)(void* buf, usize str);
};

// ext2 keeps its state small enough to share the union with fatfs
struct ex2file {
    u32 ino;
    off_t pos;
};

struct ex2dir {
    u32 ino;
    off_t pos;
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
        struct ex2file exfile;
        struct ex2dir exdir;
    } data;
};

struct fdinfo* getnewfd(struct fdinfo* info);
int getfd(int fd, struct fdinfo** info);
int closefd(int fd);
int close(int fd);