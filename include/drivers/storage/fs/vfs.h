#pragma once
#include <core/std.h>

// these are literally just
// copied from EXT2 lol
#define S_IFSOCK 0xC000
#define S_IFLNK  0xA000
#define S_IFREG  0x8000 // handled
#define S_IFBLK  0x6000
#define S_IFDIR  0x4000 // handled
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

#define MAJOR(dev) ((u32)((dev) >> 20))
#define MINOR(dev) ((u32)((dev) & ((1U << 20) - 1)))
#define MKDEV(maj, min) (((maj) << 20) | (min))

typedef struct {
    u16 mode;
    u16 uid;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 rdev;
    u16 gid;
    u16 lnkcnt; // tracked by fs driver
    u64 size; // tracked by fs driver
    u64 priv;
} vinode_t;

struct vfsops;
typedef struct {
    char path[1024];
    u32 root_ino; // filled by mount
    struct vfsops* ops; // filed by mount
    u32 blkid;
    usize mntno;
    u64 priv; // filled by mount
    int inuse;
} vfs_t;

typedef struct vfsops {
    ssize (*umount)(vfs_t* vfs); // filesystem-specific unmount stuff

    ssize (*lookup)(vfs_t* vfs, u32 dino, const char* name); // lookup something inside a directory
    ssize (*readdir)(vfs_t* vfs, u32 dino, u64* prv, char* name, usize namlen, vinode_t* buf); // read a directory, prv should start as 0 and be updated by fs driver

    ssize (*mknod)(vfs_t* vfs, u16 mode, u16 uid, u16 gid, u32 rdev);
    ssize (*mkino)(vfs_t* vfs, u16 mode, u16 uid, u16 gid); // create inode
    ssize (*rmino)(vfs_t* vfs, u32 ino);
    ssize (*getino)(vfs_t* vfs, u32 ino, vinode_t* buf); // get vinode
    ssize (*setino)(vfs_t* vfs, u32 ino, vinode_t* vinod); // set ino stuff

    ssize (*mklink)(vfs_t* vfs, u32 ino, u16 mode, u32 dino, const char* name); // create link
    ssize (*rmlink)(vfs_t* vfs, u32 dino, const char* name); // remove link

    ssize (*trunc)(vfs_t* vfs, u32 ino); // truncate ino completely
    ssize (*read)(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf); // read nb bytes from ino at offset off into buf
    ssize (*write)(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf); // write nb bytes to ino at offset off from buf
} vfsops_t;

int vfs_init();