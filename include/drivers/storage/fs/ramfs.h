#pragma once
#include <drivers/storage/fs/vfs.h>
#include <stdalign.h>

typedef struct {
    u8 used;
    u16 mode;
    u16 uid;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u16 gid;
    u64 size;
    u64 allocd;
    void* dptr;
} ramfs_inode;

typedef struct {
    u8 used;
    u32 inode;
    u32 namlen;
    char* namep;
} ramfs_dirent;

typedef struct {
    ramfs_inode* inodtbl;
    u32 ninodes;
} ramfs_info;

#define RAMFS(VFS) ((ramfs_info*)((VFS)->priv))

int ramfs_mount(vfs_t* vfs);