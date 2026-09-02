#include <drivers/storage/fs/vfs.h>
#include <drivers/storage/fs/ramfs.h>
#include <lib/string.h>
#include <core/liballoc.h>
#include <core/errno.h>
#include <core/printf.h>

static u32 findfreeino(vfs_t* vfs) {
    ramfs_info* fs = RAMFS(vfs);
    for (usize i = 0; i < fs->ninodes; i++) {
        if (!fs->inodtbl[i].used) {
            return i+1;
        }
    }

    ramfs_inode* ninodtbl = realloc(fs->inodtbl, sizeof(ramfs_inode) * (fs->ninodes + 64));
    if (!ninodtbl) return 0;
    fs->inodtbl = ninodtbl;
    u32 ino = fs->ninodes + 1;
    fs->ninodes += 64;
    return ino;
}

static ramfs_inode* getinod(vfs_t* vfs, u32 ino) {
    ramfs_info* fs = RAMFS(vfs);
    if (ino >= fs->ninodes) {
        return NULL;
    }

    if (fs->inodtbl[ino-1].used) {
        return &fs->inodtbl[ino-1];
    } else {
        serial_printf("inode %d does not exist on tmpfs\n", ino);
        return NULL;
    }
}

ssize ramfs_umount(vfs_t* vfs) {
    ramfs_info* fs = RAMFS(vfs);
    for (usize i = 0; i < fs->ninodes; i++) {
        if (fs->inodtbl[i].used) {
            free(fs->inodtbl[i].dptr);
        }
    }
    free(fs->inodtbl);
    free(fs);
    free(vfs->ops);
    return 0;
}

static ssize base_mkino(vfs_t* vfs, u32 ino, ramfs_inode* inod) {
    ramfs_info* fs = RAMFS(vfs);
    if (S_TYPE(inod->mode) != S_IFCHR && S_TYPE(inod->mode) != S_IFBLK) {
        void* dptr = malloc(1024);
        if (!dptr) return -ENOMEM;
        inod->allocd = 1024;
        inod->dptr = dptr;
    } else {
        inod->allocd = 0;
        inod->dptr = NULL; // if this is a devnod we dont want to have actual
                           // backing for it, just inodes and links
    }

    fs->inodtbl[ino-1] = *inod;
    return ino;
}

ssize ramfs_mkino(vfs_t* vfs, u16 mode, u16 uid, u16 gid) {
    u32 ino = findfreeino(vfs);
    if (!ino) return -ENOMEM;
    ramfs_inode inod = (ramfs_inode){
        1, mode, uid, 0, 0, 
        0, gid, 0, 0, 0, NULL
    };
    return base_mkino(vfs, ino, &inod);
}

ssize ramfs_rmino(vfs_t* vfs, u32 ino) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;
    free(inod->dptr);
    memset(inod, 0, sizeof(*inod));
    return 0;
}

ssize ramfs_mklink(vfs_t* vfs, u32 ino, u16 mode, u32 dino, const char* name) {
    (void)mode;
    
    ramfs_inode* dinod = getinod(vfs, dino);
    if (!dinod) return -ENOENT;

    char* np = malloc(strlen(name) + 1);
    if (!np) return -ENOMEM;
    memcpy(np, name, strlen(name) + 1);

    ramfs_dirent ent = {
        1, ino, strlen(name) + 1, np
    };

    usize ndirs = dinod->size / sizeof(ramfs_dirent);
    ramfs_dirent* dir = dinod->dptr;
    ramfs_dirent* tgtent = NULL;
    for (usize i = 0; i < ndirs; i++) {
        if (!dir[i].used) {
            tgtent = &dir[i];
            break;
        }
    }

    if (!tgtent) {
        if (dinod->size + sizeof(ent) > dinod->allocd) {
            void* nptr = realloc(dinod->dptr, dinod->allocd + (sizeof(ramfs_dirent) * 32));
            if (!nptr) return -ENOMEM;

            dinod->dptr = nptr;
            dir = dinod->dptr;

            tgtent = &dir[dinod->allocd / sizeof(ramfs_dirent)];
            dinod->allocd += sizeof(ramfs_dirent) * 32;
        } else {
            tgtent = &dir[dinod->size / sizeof(ramfs_dirent)];
        }
    }

    memcpy(tgtent, &ent, sizeof(ent));
    dinod->size += sizeof(ent);
    return 0;
}

ssize ramfs_rmlink(vfs_t* vfs, u32 dino, const char* name) {
    usize namlen = strlen(name) + 1;

    ramfs_inode* dinod = getinod(vfs, dino);
    if (!dinod) return -ENOENT;

    usize ndirs = dinod->size / sizeof(ramfs_dirent);
    ramfs_dirent* dir = dinod->dptr;
    for (usize i = 0; i < ndirs; i++) {
        if (dir[i].used && dir[i].namlen == namlen && strneq(dir[i].namep, name, namlen)) {
            free(dir[i].namep);
            dir[i].namlen = 0;
            dir[i].inode = 0;
            dir[i].used = 0;
        }
    }

    return -ENOENT;
}

ssize ramfs_trunc(vfs_t* vfs, u32 ino) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;

    memset(inod->dptr, 0, inod->size);
    inod->size = 0;
    return 0;
}

ssize ramfs_getino(vfs_t* vfs, u32 ino, vinode_t* buf) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;

    buf->mode = inod->mode;
    buf->uid = inod->uid;
    buf->atime = inod->atime;
    buf->ctime = inod->ctime;
    buf->mtime = inod->mtime;
    buf->gid = inod->gid;
    buf->lnkcnt = 0;
    buf->size = inod->size;
    buf->priv = ino;
    buf->rdev = inod->rdev;

    return 0;
}

ssize ramfs_setino(vfs_t* vfs, u32 ino, vinode_t* vinod) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;

    inod->mode = vinod->mode;
    inod->uid = vinod->uid;
    inod->atime = vinod->atime;
    inod->ctime = vinod->ctime;
    inod->mtime = vinod->mtime;
    inod->gid = vinod->gid;

    if (S_TYPE(vinod->mode) == S_IFCHR || S_TYPE(vinod->mode) == S_IFBLK) {
        inod->rdev = vinod->rdev;
    }

    return 0;
}

ssize ramfs_lookup(vfs_t* vfs, u32 dino, const char* name) {
    usize namlen = strlen(name) + 1;

    ramfs_inode* dinod = getinod(vfs, dino);
    if (!dinod) return -ENOENT;

    usize ndirs = dinod->size / sizeof(ramfs_dirent);
    ramfs_dirent* dir = dinod->dptr;
    for (usize i = 0; i < ndirs; i++) {
        if (dir[i].used && dir[i].namlen == namlen && strneq(dir[i].namep, name, namlen)) {
            return dir[i].inode;
        }
    }

    return -ENOENT;
}

ssize ramfs_readdir(vfs_t* vfs, u32 dino, u64* prv, char* name, usize namlen, vinode_t* buf) {
    ramfs_inode* dinod = getinod(vfs, dino);
    if (!dinod) return -ENOENT;

    usize ndirs = dinod->size / sizeof(ramfs_dirent);
    ramfs_dirent* dir = dinod->dptr;

    serial_printf("ramfs_readdir: dino=%u size=%zu ndirs=%zu pos=%llu\n",
        dino, dinod->size, ndirs, *prv);

    if (*prv >= ndirs) {
        return -1;
    }

    if (namlen < dir[*prv].namlen) return -ETOOSMALL;
    memcpy(name, dir[*prv].namep, dir[*prv].namlen);
    ramfs_inode* inod = getinod(vfs, dir[*prv].inode);
    
    buf->mode = inod->mode;
    buf->uid = inod->uid;
    buf->atime = inod->atime;
    buf->ctime = inod->ctime;
    buf->mtime = inod->mtime;
    buf->gid = inod->gid;
    buf->lnkcnt = 0;
    buf->size = inod->size;
    buf->priv = dir[*prv].inode;
    buf->rdev = inod->rdev;
    (*prv)++;
    return 0;
}

ssize ramfs_read(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;

    if (inod->size < off) {
        return -1;
    }

    if (inod->size < off + nb) {
        nb = inod->size - off;
    }

    memcpy(buf, inod->dptr + off, nb);
    return nb;
}

ssize ramfs_write(vfs_t* vfs, u32 ino, usize off, usize nb, void* buf) {
    ramfs_inode* inod = getinod(vfs, ino);
    if (!inod) return -ENOENT;

    if (inod->allocd < off + nb) {
        void* ndata = realloc(inod->dptr, inod->allocd + nb);
        if (!ndata) return -ENOMEM;
        inod->dptr = ndata;
    }

    memcpy(inod->dptr + off, buf, nb);
    return nb;
}

ssize ramfs_mknod(vfs_t* vfs, u16 mode, u16 uid, u16 gid, u32 rdev) {
    u32 ino = findfreeino(vfs);
    if (!ino) return -ENOMEM;
    ramfs_inode inod = {
        1, mode, uid, 0, 0, 
        0, gid, 0, rdev, 0, NULL
    };
    return base_mkino(vfs, ino, &inod);
}

int ramfs_mount(vfs_t* vfs) {
    vfs->root_ino = 1;
    ramfs_info* fs = malloc(sizeof(*fs));
    if (!fs) return -ENOMEM;

    fs->inodtbl = malloc(sizeof(*fs->inodtbl) * 1024);
    if (!fs->inodtbl) {
        free(fs);
        return -ENOMEM;
    }

    memset(fs->inodtbl, 0, sizeof(*fs->inodtbl) * 1024);
    fs->ninodes = 1024;

    vfs->priv = (u64)fs;

    vfs->ops = malloc(sizeof(*vfs->ops));
    if (!vfs->ops) {
        free(fs->inodtbl);
        free(fs);
        return -ENOMEM;
    }

    vfs->ops->umount = ramfs_umount;
    vfs->ops->lookup = ramfs_lookup;
    vfs->ops->readdir = ramfs_readdir;
    vfs->ops->mkino = ramfs_mkino;
    vfs->ops->rmino = ramfs_rmino;
    vfs->ops->getino = ramfs_getino;
    vfs->ops->setino = ramfs_setino;
    vfs->ops->mklink = ramfs_mklink;
    vfs->ops->rmlink = ramfs_rmlink;
    vfs->ops->trunc = ramfs_trunc;
    vfs->ops->read = ramfs_read;
    vfs->ops->write = ramfs_write;
    vfs->ops->mknod = ramfs_mknod;

    ramfs_inode root_inod = {
        1, S_IFDIR | 0755, 0, 0, 0, 
        0, 0, 0, 0, 0, NULL
    };

    int ret = 0;
    if ((ret = base_mkino(vfs, 1, &root_inod)) < 0) {
        free(fs->inodtbl);
        free(fs);
        free(vfs->ops);
        return -ENOMEM;
    }

    ramfs_mklink(vfs, 1, root_inod.mode, 1, ".");
    ramfs_mklink(vfs, 1, root_inod.mode, 1, "..");
    
    return 0;
}