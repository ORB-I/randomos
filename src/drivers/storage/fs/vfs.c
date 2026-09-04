#include <core/std.h>
#include <core/kprint.h>
#include <core/printf.h>
#include <core/errno.h>
#include <lib/string.h>
#include <core/liballoc.h>
#include <drivers/storage/fs.h>
#include <drivers/display/serial.h>
#include <drivers/display/term.h>
#include <drivers/hid/kbd.h>
#include <core/fd.h>
#include <drivers/storage/fs/vfs.h>
#include <drivers/storage/block/block.h>
#include <drivers/storage/fs/ext2.h>
#include <drivers/storage/fs/ramfs.h>
#include <scheduler/process.h>

int initramfs_mount(vfs_t* vfs);

#define VFS_PATH_MAX     4096
#define VFS_NAME_MAX     255
#define VFS_MAX_SYMLINKS 40
#define VFS_NO_SYMLINKS  0x01
#define VFS_MUST_BE_DIR  0x02
#define VFS_FOLLOW_FINAL 0x04

typedef struct {
    char* items[VFS_PATH_MAX / 2];
    usize cnt;
} pathstk_t;

char cwd[1024];

static pathstk_t* pathstk_init() {
    pathstk_t* stk = malloc(sizeof(*stk));
    if (!stk) return NULL;
    stk->cnt = 0;
    memset(stk->items, 0, sizeof(stk->items));
    return stk;
}

static int pathstk_push(pathstk_t* stk, char* comp) {
    if (stk->cnt >= VFS_PATH_MAX / 2) {
        return -EINVAL;
    }

    stk->items[stk->cnt++] = comp;
    return 0;
}

static void pathstk_pop(pathstk_t* stk) {
    if (stk->cnt) {
        stk->cnt--;
    }
}

ssize vfs_resolve(vfs_t* mnt, const char* path, u32 flags) {
    if (!mnt || !path || *path == '\0') {
        return -EINVAL;
    }

    char* pbuf = malloc(VFS_PATH_MAX);
    if (!pbuf) return -ENOMEM;
    char* lnkbuf = malloc(VFS_PATH_MAX);
    if (!lnkbuf) {
        free(pbuf);
        return -ENOMEM;
    }

    pathstk_t* stk = pathstk_init();
    if (!stk) {
        free(lnkbuf);
        free(pbuf);
        return -ENOMEM;
    }

    const char* rel_path = path;
    usize mntlen = strlen(mnt->path);
    if (strncmp(rel_path, mnt->path, mntlen) == 0) {
        rel_path += mntlen;
        if (*rel_path == '/') {
            rel_path++;
        }
    }

    usize len = strlen(rel_path);
    if (len >= VFS_PATH_MAX) {
        free(stk);
        free(lnkbuf);
        free(pbuf);
        return -ENAMETOOLONG;
    }

    memcpy(pbuf, rel_path, len + 1);

    {
        pathstk_t* comps = pathstk_init();
        if (!comps) {
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return -ENOMEM;
        }

        char* p = pbuf;
        while (*p) {
            while (*p == '/') p++;
            if (*p == '\0') break;

            char* st = p;
            while (*p != '/' && *p != '\0') p++;

            if (p - st > VFS_NAME_MAX) {
                free(stk);
                free(comps);
                free(lnkbuf);
                free(pbuf);
                return -ENAMETOOLONG;
            }

            if (*p) {
                *p++ = '\0';
            }

            if (pathstk_push(comps, st) < 0) {
                free(stk);
                free(comps);
                free(lnkbuf);
                free(pbuf);
                return -ENAMETOOLONG;
            }
        }

        for (usize i = comps->cnt; i > 0; i--) {
            int ret = pathstk_push(stk, comps->items[i - 1]);
            if (ret < 0) {
                free(stk);
                free(comps);
                free(lnkbuf);
                free(pbuf);
                return ret;
            }
        }
        free(comps);
    }

    u32 ino = mnt->root_ino;
    usize symlnks = 0;
    usize depth = 0;
    pathstk_t* done = pathstk_init();
    if (!done) {
        free(stk);
        free(lnkbuf);
        free(pbuf);
        return -ENOMEM;
    }

    while (stk->cnt) {
        char* comp = stk->items[stk->cnt - 1];
        stk->cnt--;

        if (streq(comp, ".")) continue;

        if (streq(comp, "..")) {
            if (depth == 0) {
                ino = mnt->root_ino;
                continue;
            }

            ssize ret = mnt->ops->lookup(mnt, ino, "..");
            if (ret < 0) {
                free(done);
                free(stk);
                free(lnkbuf);
                free(pbuf);
                return ret;
            }

            ino = (u32)ret;
            depth--;

            pathstk_pop(done);
            continue;
        }

        ssize ret = mnt->ops->lookup(mnt, ino, comp);
        if (ret < 0) {
            free(done);
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return ret;
        }

        u32 nxt = ret;
        vinode_t inod;
        if ((ret = mnt->ops->getino(mnt, nxt, &inod)) < 0) {
            free(done);
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return ret;
        }

        bool final = stk->cnt == 0;
        if (S_TYPE(inod.mode) == S_IFLNK) {
            if (flags & VFS_NO_SYMLINKS) {
                free(done);
                free(stk);
                free(lnkbuf);
                free(pbuf);
                return -ELOOP;
            }

            if (final && !(flags & VFS_FOLLOW_FINAL)) {
                ino = nxt;
                depth++;
                if (flags & VFS_MUST_BE_DIR) {
                    free(done);
                    free(stk);
                    free(lnkbuf);
                    free(pbuf);
                    return -ENOTDIR;
                }

                free(done);
                free(stk);
                free(lnkbuf);
                free(pbuf);
                return ino;
            }

            if (++symlnks > VFS_MAX_SYMLINKS) {
                free(done);
                free(stk);
                free(lnkbuf);
                free(pbuf);
                return -ELOOP;
            }
            
            if ((ret = mnt->ops->readsym(mnt, nxt, lnkbuf, VFS_PATH_MAX)) < 0) {
                free(done);
                free(stk);
                free(lnkbuf);
                free(pbuf);
                return ret;
            }
            if (lnkbuf[0] == '/') {
                ino = mnt->root_ino;
                depth = 0;
                free(done);
                done = pathstk_init();
                if (!done) {
                    free(stk);
                    free(lnkbuf);
                    free(pbuf);
                    return -ENOMEM;
                }
            }

            {
                pathstk_t* tgtcomps = pathstk_init();
                if (!tgtcomps) {
                    free(done);
                    free(stk);
                    free(lnkbuf);
                    free(pbuf);
                    return -ENOMEM;
                }

                char* tp = lnkbuf;
                while (*tp) {
                    while (*tp == '/') tp++;
                    if (*tp == '\0') break;

                    char* st = tp;
                    while (*tp != '/' && *tp != '\0') tp++;
                    if (tp - st > VFS_NAME_MAX) {
                        free(tgtcomps);
                        free(done);
                        free(stk);
                        free(lnkbuf);
                        free(pbuf);
                        return -ENAMETOOLONG;
                    }
                    
                    if (*tp) *tp++ = '\0';
                    if (pathstk_push(tgtcomps, st) < 0) {
                        free(tgtcomps);
                        free(done);
                        free(stk);
                        free(lnkbuf);
                        free(pbuf);
                        return -ENAMETOOLONG;
                    }
                }

                usize ocnt = stk->cnt;
                if (ocnt + tgtcomps->cnt > VFS_PATH_MAX / 2) {
                    free(tgtcomps);
                    free(done);
                    free(stk);
                    free(lnkbuf);
                    free(pbuf);
                    return -ENAMETOOLONG;
                }

                for (usize i = ocnt; i > 0; i--) {
                    stk->items[i + tgtcomps->cnt - 1] = stk->items[i - 1];
                }

                for (usize i = 0; i < tgtcomps->cnt; i++) {
                    stk->items[i] = tgtcomps->items[tgtcomps->cnt - 1 - i];
                }
                free(tgtcomps);
            }

            continue;
        }

        ino = nxt;
        depth++;
        if (done->cnt >= VFS_PATH_MAX / 2) {
            free(done);
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return -ENAMETOOLONG;
        }
        
        pathstk_push(done, comp);
    }

    if (flags & VFS_MUST_BE_DIR) {
        int ret = 0;
        vinode_t inod;
        if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) {
            free(done);
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return ret;
        }

        if (S_TYPE(inod.mode) != S_IFDIR) {
            free(done);
            free(stk);
            free(lnkbuf);
            free(pbuf);
            return -ENOTDIR;
        }
    }

    free(done);
    free(stk);
    free(lnkbuf);
    free(pbuf);
    return ino;
}

static vfs_t* mounts = NULL;
usize avmounts = 0;

#define FSFLAG_NOBLK 0x01
struct av_vfs {
    const char* id;
    u32 flags;
    int (*mount)(vfs_t* vfs);
};

struct av_vfs availfs[] = {
    {"ext2", 0, ext2fs_mount},
    {"ramfs", FSFLAG_NOBLK, ramfs_mount},
    {"initramfs", FSFLAG_NOBLK, initramfs_mount}
};
static const usize navailfs = sizeof(availfs) / sizeof(availfs[0]);

int vfs_init() {
    mounts = malloc(sizeof(*mounts) * 16);
    if (!mounts) return -ENOMEM;
    memset(mounts, 0, sizeof(*mounts) * 16);
    avmounts = 16;
    memcpy(cwd, "/", 2);
    return 0;
}

int vfs_canonicalize(const char* path, char* out, usize outlen) {
    if (!path || !out || outlen == 0 || *path == '\0') return -EINVAL;

    usize opos = 0;
    out[opos++] = '/';

    while (*path) {
        while (*path == '/') path++;
        if (*path == '\0') break;

        const char* st = path;
        usize len = 0;

        while (path[len] != '/' && path[len] != '\0') len++;

        path += len;

        if (len == 1 && st[0] == '.') {
            continue;
        }

        if (len == 2 && st[0] == '.' && st[1] == '.') {
            if (opos > 1) {
                opos--;
                while (opos > 0 && out[opos - 1] != '/') {
                    opos--;
                }
            }
            continue;
        }

        if (opos > 0 && out[opos-1] != '/') {
            if (opos + 1 >= outlen) return -ETOOSMALL;
            out[opos++] = '/';
        }

        if (opos + len >= outlen) return -ETOOSMALL;
        memcpy(out + opos, st, len);
        opos += len;
    }

    if (opos == 0) {
        out[0] = '/';
        opos = 1;
    }

    out[opos] = '\0';
    return 0;
}

int vfs_abspath(const char* path, char* out, usize outlen) {
    if (!path || !out || outlen == 0 || *path == '\0') return -EINVAL;
    char tmp[1024];
    usize len;

    if (path[0] == '/') {
        len = strlen(path);
        if (len + 1 > sizeof(tmp)) return -ETOOSMALL;
        memcpy(tmp, path, len + 1);
    } else {
        usize cwdlen = strlen(cwd);
        usize pathlen = strlen(path);

        if (cwdlen + 1 + pathlen + 1 > sizeof(tmp)) {
            return -ETOOSMALL;
        }

        memcpy(tmp, cwd, cwdlen);
        if (cwdlen > 0 && tmp[cwdlen-1] != '/') {
            tmp[cwdlen++] = '/';
        }

        memcpy(tmp + cwdlen, path, pathlen + 1);
    }

    return vfs_canonicalize(tmp, out, outlen);
}

vfs_t* vfs_getmnt(const char* path) {
    vfs_t* mnt = NULL;
    usize mntlen = 0;

    char abs[1024];
    if (vfs_abspath(path, abs, 1024) < 0) return NULL;

    for (usize i = 0; i < avmounts; i++) {
        vfs_t* m = &mounts[i];
        if (!m->inuse) continue;

        usize len = strlen(m->path);
        if (len < mntlen) continue;
        if (strncmp(abs, m->path, len) != 0) continue;

        if (len > 1 && abs[len] != '\0' && abs[len] != '/') continue;

        mnt = m;
        mntlen = len;
    }

    kprint("Resolved %s to mount %s (id %zu)\n", path, (mnt) ? mnt->path : "none", (mnt) ? mnt->mntno : 0);
    return mnt;
}

ssize vfs_findfreemnt() {
    ssize mntid = -1;
    for (usize i = 0; i < avmounts; i++) {
        if (!mounts[i].inuse) {
            mntid = i;
            break;
        }
    }

    if (mntid < 0) {
        vfs_t* nmounts = realloc(mounts, sizeof(*mounts) * (avmounts + 4));
        if (!nmounts) return -ENOMEM;
        memset(nmounts + avmounts, 0, sizeof(*mounts) * 4);
        mounts = nmounts;
        avmounts += 4;

        return avmounts - 3;
    } else {
        return mntid;
    }
}

int vfs_dirname(const char* path, char* dir, usize dirlen) {
    if (!path || !dir || dirlen == 0) return -EINVAL;
    usize len = strlen(path);

    while (len > 0 && path[len-1] == '/') len--;
    if (len == 0) {
        if (dirlen < 2) return -ETOOSMALL;
        dir[0] = '/'; dir[1] = '\0';
        return 0;
    }

    usize slash = len;
    while (slash > 0 && path[slash-1] != '/') slash--;

    if (slash == 0) {
        if (dirlen < 2) return -ETOOSMALL;
        dir[0] = '.'; dir[1] = '\0';
        return 0;
    }

    if (slash == 1 && path[0] == '/') {
        if (dirlen < 2) return -ETOOSMALL;
        dir[0] = '/'; dir[1] = '\0';
        return 0;
    }

    usize dlen = slash - 1;
    if (dirlen <= dlen) return -ETOOSMALL;

    memcpy(dir, path, dlen);
    dir[dlen] = '\0';
    return 0;
}

int vfs_basename(const char* path, char* base, usize baselen) {
    if (!path || !base || baselen == 0) return -EINVAL;
    usize len = strlen(path);

    while (len > 0 && path[len-1] == '/')
        len--;

    if (len == 0) {
        if (baselen < 1) return -ETOOSMALL;
        base[0] = '\0';
        return 0;
    }

    usize slash = len;
    while (slash > 0 && path[slash-1] != '/') slash--;

    const char* bname = path + slash;
    usize bnamlen = len - slash;

    if (baselen <= bnamlen) return -ETOOSMALL;

    memcpy(base, bname, bnamlen);
    base[bnamlen] = '\0';
    return 0;
}

int vfs_basecreat(const char* path, int mode, u64* inop) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;
    
    char dir[1024], base[1024];
    if ((ret = vfs_dirname(abs, dir, 1024)) < 0) return ret;
    if ((ret = vfs_basename(abs, base, 1024)) < 0) return -1;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize dino = 0;
    if ((dino = vfs_resolve(mnt, dir, VFS_FOLLOW_FINAL)) < 0) return dino;

    ssize _fino = 0;
    if ((_fino = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL)) >= 0) return -EEXISTS;

    ssize ino = 0;
    if ((ino = mnt->ops->mkino(mnt, mode, proctbl[current_pid].euid, proctbl[current_pid].egid)) < 0) return ino;

    if ((ret = mnt->ops->mklink(mnt, ino, mode, dino, base)) < 0) {
        return ret;
    }

    *inop = ino;
    return 0;
}

ssize vfs_getdino(vfs_t* mnt, const char* path) {
    int ret = 0;
    
    char dir[1024];
    if ((ret = vfs_dirname(path, dir, sizeof(dir))) < 0) return ret;

    ssize dino = 0;
    if ((dino = vfs_resolve(mnt, dir, VFS_MUST_BE_DIR | VFS_FOLLOW_FINAL)) < 0) return dino;

    return dino;
}

int vfs_baseunlink(vfs_t* mnt, u64 ino, const char* path) {
    int ret = 0;

    ssize dino = 0;
    if ((dino = vfs_getdino(mnt, path)) < 0) return dino;

    char base[1024];
    if ((ret = vfs_basename(path, base, sizeof(base))) < 0) return ret;

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    if (inod.lnkcnt > 1) {
        if ((ret = mnt->ops->rmlink(mnt, dino, base)) < 0) return ret;
    } else {
        if ((ret = mnt->ops->rmlink(mnt, dino, base)) < 0) return ret;
        if ((ret = mnt->ops->rmino(mnt, ino)) < 0) return ret;
    }

    return 0;
}

int mount(const char* dev, const char* path, const char* type) {
    int ret = 0;

    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    ssize mntid = vfs_findfreemnt();
    if (mntid < 0) return mntid;

    struct av_vfs* fs = NULL;
    for (usize i = 0; i < navailfs; i++) {
        if (streq(type, availfs[i].id)) {
            fs = &availfs[i];
        }
    }

    if (!fs) {
        return -EINVAL;
    }

    vfs_t* mnt = &mounts[mntid];
    mnt->inuse = 1;

    if (!(fs->flags & FSFLAG_NOBLK)) {
        struct blockdev bdev;
        if (dev) {
            if ((ret = block_getdevnam(dev, &bdev)) < 0) {
                mnt->inuse = 0;
                return ret;
            }
        } else {
            struct blockdev* devs = block_getdevs();
            usize ndevs = block_getndevs();
            if (ndevs >= 0) {
                bdev = devs[0];
            } else {
                mnt->inuse = 1;
                return -ENOEXIST;
            }
        }
        mnt->blkid = bdev.id;
    }
    
    mnt->mntno = mntid;
    memcpy(mnt->path, abs, strlen(abs)+1);

    if ((ret = fs->mount(mnt)) < 0) {
        mnt->blkid = 0;
        mnt->mntno = 0;
        memset(mnt->path, 0, sizeof(mnt->path));
        mnt->inuse = 0;
        return ret;
    }

    kprint("Mounted device %s at %s (type %s mountid %zu)\n", (fs->flags & FSFLAG_NOBLK) ? "ram" : dev, path, type, mntid);

    return 0;
}

int umount(const char* path) {
    vfs_t* mnt = vfs_getmnt(path);
    mnt->ops->umount(mnt);
    mnt->ops = NULL;
    mnt->blkid = 0;
    mnt->priv = 0;
    mnt->mntno = 0;
    memset(mnt->path, 0, sizeof(mnt->path));
    mnt->inuse = 0;

    return 0;
}

int mknod(const char* path, u32 dev, int mode) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;
    
    char dir[1024], base[1024];
    if ((ret = vfs_dirname(abs, dir, 1024)) < 0) return ret;
    if ((ret = vfs_basename(abs, base, 1024)) < 0) return -1;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize dino = 0;
    if ((dino = vfs_resolve(mnt, dir, VFS_FOLLOW_FINAL)) < 0) return dino;

    ssize _fino = 0;
    if ((_fino = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL)) >= 0) return -EEXISTS;

    ssize ino = 0;
    if ((ino = mnt->ops->mknod(mnt, mode, proctbl[current_pid].euid, proctbl[current_pid].egid, dev)) < 0) return ino;

    kprint("Linking inode %zu to directory %zu\n", ino, dino);
    if ((ret = mnt->ops->mklink(mnt, ino, mode, dino, base)) < 0) {
        return ret;
    }

    return 0;
}

int creat(const char* path, int mode) {
    u64 inop = 0;
    return vfs_basecreat(path, mode | S_IFREG, &inop);
}

int open(const char* path, int flags, u16 mode) {
    if (flags & O_CREAT) {
        mode |= S_IFREG;
    }

    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize ino = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL);
    if (ino == -ENOENT) {
        if (flags & O_CREAT) {
            if ((ret = vfs_basecreat(abs, mode | S_IFREG, (u64*)&ino)) < 0) {
                return ret;
            }
        } else {
            return -ENOENT;
        }
    } else if (ino < 0) {
        return ino;
    }

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    if (S_TYPE(inod.mode) == S_IFBLK || S_TYPE(inod.mode) == S_IFCHR) {
        struct fdinfo info = {
            0, 0, FDTYPE_DEV, {.dev = {
                inod.mode, inod.rdev
            }}
        };

        struct fdinfo* ninfo = NULL;
        if (!(ninfo = getnewfd(&info))) {
            return -ENOMEM;
        }

        return ninfo->fd;
    } else {
        struct fdinfo info = {
            0, 0, FDTYPE_FILE, {.file = {
                mnt, inod, ino, 0, 0, {0}
            }}
        };

        if (flags & O_APPEND) {
            info.data.file.pos = inod.size;
        }

        struct fdinfo* ninfo = NULL;
        if (!(ninfo = getnewfd(&info))) {
            return -ENOMEM;
        }

        if (flags & O_TRUNC) {
            if ((ret = mnt->ops->trunc(mnt, ino)) < 0) {
                closefd(ninfo->fd);
                return ret;
            }
        }

        return ninfo->fd;
    }
}

ssize fsread(int fd, void* buf, usize sz) {
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    ssize nread = info->data.file.mnt->ops->read(info->data.file.mnt, info->data.file.ino, info->data.file.pos, sz, buf);
    if (nread < 0) {
        return nread;
    } else {
        info->data.file.pos += nread;
        return nread;
    }
}

ssize fswrite(int fd, void* buf, usize sz) {
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    ssize nwritten = info->data.file.mnt->ops->write(info->data.file.mnt, info->data.file.ino, info->data.file.pos, sz, buf);
    if (nwritten< 0) {
        return nwritten;
    } else {
        info->data.file.pos += nwritten;
        return nwritten;
    }
}

off_t lseek(int fd, off_t off, int whence) {
    int ret = 0;
    struct fdinfo* fdinfo;
    if ((ret = getfd(fd, &fdinfo)) < 0) return ret;

    if (fdinfo->type != FDTYPE_FILE) return -EBADF;
    struct file* ent = &fdinfo->data.file;

    if (whence == SEEK_SET) {
        if ((u64)off > ent->inod.size) return -ERANGE;
        ent->pos = off;
        return ent->pos;
    } else if (whence == SEEK_CUR) {
        if (ent->pos + off > ent->inod.size) return -ERANGE;
        ent->pos += off;
        return ent->pos;
    } else if (whence == SEEK_END) {
        if (off > 0) return -ERANGE;
        if (ent->inod.size + off < 0) return -ERANGE;
        ent->pos = ent->inod.size + off;
        return ent->pos;
    } else {
        return -EINVAL;
    }
}

int trunc(int fd) {
    int ret = 0;

    struct fdinfo* fdinfo;
    if ((ret = getfd(fd, &fdinfo)) < 0) return ret;

    if (fdinfo->type != FDTYPE_FILE) return -EBADF;
    struct file* ent = &fdinfo->data.file;

    return ent->mnt->ops->trunc(ent->mnt, ent->ino);
}

int sync(int fd) {
    (void)fd;
    return 0;
}

int opendir(const char* path) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize ino = 0;
    if ((ino = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL | VFS_MUST_BE_DIR)) < 0) {
        return ino;
    }

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) {
        return ret;
    }

    if (S_TYPE(inod.mode) != S_IFDIR) {
        return -ENOTDIR;
    }

    struct fdinfo info = {
        0, 0, FDTYPE_DIR, {.dir = {
            mnt, inod, ino, 0, 0, {0}
        }}
    };
    memcpy(info.data.dir.path, abs, strlen(abs)+1);

    struct fdinfo* ninfo = NULL;
    if (!(ninfo = getnewfd(&info))) {
        return -ENOMEM;
    }

    return ninfo->fd;
}

int closedir(int dd) {
    return closefd(dd);
}

int readdir(int dd, struct stat* st) {
    int ret = 0;

    struct fdinfo* fdinfo;
    if ((ret = getfd(dd, &fdinfo)) < 0) {
        kprint("(1) readddir returning %d\n", ret);
        return ret;
    }

    if (fdinfo->type != FDTYPE_DIR) {
        kprint("(2) readddir returning %d\n", -ENOTDIR);
        return -ENOTDIR;
    }
    struct file* ent = &fdinfo->data.dir;

    vinode_t inod;
    char name[1024];
    ssize fsino = 0;
    if ((fsino = ent->mnt->ops->readdir(ent->mnt, ent->ino, &ent->pos, name, 1024, &inod)) >= 0) {
        char child_path[1024];
        if (streq(ent->path, "/")) {
            snprintf(child_path, sizeof(child_path), "/%s", name);
        } else {
            snprintf(child_path, sizeof(child_path), "%s/%s", ent->path, name);
        }

        for (usize i = 0; i < avmounts; i++) {
            if (mounts[i].inuse && streq(mounts[i].path, child_path)) {
                vinode_t minod;
                memset(&minod, 0, sizeof(minod));
                if (mounts[i].ops && mounts[i].ops->getino) {
                    if (mounts[i].ops->getino(&mounts[i], mounts[i].root_ino, &minod) >= 0) {
                        inod = minod;
                        fsino = mounts[i].root_ino;
                    }
                }
                break;
            }
        }

        memcpy(st->st_name, name, strlen(name)+1);
        st->st_uid = inod.uid;
        st->st_atime = inod.atime;
        st->st_ctime = inod.ctime;
        st->st_mtime = inod.mtime;
        st->st_gid = inod.gid;
        st->st_mode = inod.mode;
        st->st_size = inod.size;
        st->st_ino = fsino;
        return 0;
    }

    while (ent->mnt_pos < avmounts) {
        usize m_idx = ent->mnt_pos++;
        vfs_t* m = &mounts[m_idx];
        if (!m->inuse) continue;
        if (streq(m->path, "/")) continue;

        char parent[1024];
        if (vfs_dirname(m->path, parent, sizeof(parent)) < 0) continue;
        if (!streq(parent, ent->path)) continue;

        char child_name[1024];
        if (vfs_basename(m->path, child_name, sizeof(child_name)) < 0) continue;

        if (ent->mnt->ops->lookup && ent->mnt->ops->lookup(ent->mnt, ent->ino, child_name) >= 0) {
            continue;
        }

        int duplicate = 0;
        for (usize p = 0; p < m_idx; p++) {
            if (!mounts[p].inuse) continue;
            char prev_parent[1024];
            if (vfs_dirname(mounts[p].path, prev_parent, sizeof(prev_parent)) < 0) continue;
            if (!streq(prev_parent, ent->path)) continue;
            char prev_name[1024];
            if (vfs_basename(mounts[p].path, prev_name, sizeof(prev_name)) < 0) continue;
            if (streq(prev_name, child_name)) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) continue;

        vinode_t minod;
        memset(&minod, 0, sizeof(minod));
        if (m->ops && m->ops->getino) {
            m->ops->getino(m, m->root_ino, &minod);
        }

        memcpy(st->st_name, child_name, strlen(child_name)+1);
        st->st_uid = minod.uid;
        st->st_atime = minod.atime;
        st->st_ctime = minod.ctime;
        st->st_mtime = minod.mtime;
        st->st_gid = minod.gid;
        st->st_mode = minod.mode ? minod.mode : (S_IFDIR | 0755);
        st->st_size = minod.size;
        st->st_ino = m->root_ino;
        return 0;
    }

    return -1;
}

int stat(const char* path, struct stat* st) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    char base[1024];
    if ((ret = vfs_basename(abs, base, 1024)) < 0) return -1;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize ino = 0;
    if ((ino = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL)) < 0) return ino;

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    memcpy(st->st_name, base, strlen(base)+1);
    st->st_uid = inod.uid;
    st->st_atime = inod.atime;
    st->st_ctime = inod.ctime;
    st->st_mtime = inod.mtime;
    st->st_gid = inod.gid;
    st->st_mode = inod.mode;
    st->st_size = inod.size;
    st->st_ino = ino;

    return 0;
}

int unlink(const char* path) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize ino = 0;
    if ((ret = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL)) < 0) return ret;

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    if (S_TYPE(inod.mode) == S_IFDIR) {
        return -EISDIR;
    }

    return vfs_baseunlink(mnt, ino, abs);
}

int rmdir(const char* path) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(abs);

    ssize ino = 0;
    if ((ret = vfs_resolve(mnt, abs, VFS_FOLLOW_FINAL | VFS_MUST_BE_DIR)) < 0) return ret;

    vinode_t inod;
    u64 pos = 0;
    char name[1024] = {0};
    while ((ret = mnt->ops->readdir(mnt, ino, &pos, name, 1024, &inod)) >= 0) {
        usize namlen = strlen(name);
        if (!((namlen == 1 && name[0] == '.') || (namlen == 2 && name[0] == '.' && name[1] == '.'))) {
            return -ENOTEMPTY;
        }
        memset(name, 0, sizeof(name));
    }

    mnt->ops->rmlink(mnt, ino, ".");
    mnt->ops->rmlink(mnt, ino, "..");
    return vfs_baseunlink(mnt, ino, path);
}

int rename(const char* oname, const char* nname) {
    int ret = 0;
    char oabs[1024];
    if ((ret = vfs_abspath(oname, oabs, 1024)) < 0) return ret;
    char nabs[1024];
    if ((ret = vfs_abspath(nname, nabs, 1024)) < 0) return ret;

    char nbase[1024];
    if ((ret = vfs_basename(nabs, nbase, 1024)) < 0) return ret;

    char obase[1024];
    if ((ret = vfs_basename(oabs, obase, 1024)) < 0) return ret;

    vfs_t* omnt = vfs_getmnt(oabs);
    vfs_t* nmnt = vfs_getmnt(nabs);

    if (omnt->mntno != nmnt->mntno) {
        return -EINVAL;
    }

    ssize ino = 0;
    if ((ino = vfs_resolve(omnt, oabs, VFS_FOLLOW_FINAL)) < 0) return ino;

    vinode_t inode;
    if ((ret = omnt->ops->getino(omnt, ino, &inode)) < 0) return ret;

    ssize ndino = 0, odino = 0;
    if ((odino = vfs_getdino(nmnt, nname)) < 0) return odino;
    if ((ndino = vfs_getdino(nmnt, nname)) < 0) return ndino;

    if ((ret = nmnt->ops->mklink(nmnt, ino, inode.mode, ndino, nbase)) < 0) return -1;
    return omnt->ops->rmlink(omnt, odino, obase);
}

int mkdir(const char* path, int mode) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(path);
    ssize dino = vfs_getdino(mnt, abs);

    vinode_t dinod;
    if ((ret = mnt->ops->getino(mnt, dino, &dinod)) < 0) return ret;

    u64 ino = 0;
    if ((ret = vfs_basecreat(path, mode | S_IFDIR, &ino)) < 0) return ret;

    if ((ret = mnt->ops->mklink(mnt, ino, mode | S_IFDIR, ino, ".")) < 0) {
        rmdir(path);
        return ret;
    }

    if ((ret = mnt->ops->mklink(mnt, dino, dinod.mode, ino, "..")) < 0) {
        rmdir(path);
        return ret;
    }

    return 0;
}

int symlink(const char* path, const char* target, int mode) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;
    
    char abst[1024];
    if ((ret = vfs_abspath(target, abst, 1024)) < 0) return ret;

    char base[1024];
    if ((ret = vfs_basename(path, base, 1024)) < 0) return ret;

    vfs_t* mnt = vfs_getmnt(path);
    ssize dino = vfs_getdino(mnt, abs);

    ssize ino;
    if ((ino = mnt->ops->symlink(mnt, mode | S_IFLNK, proctbl[current_pid].euid, proctbl[current_pid].egid, abst)) < 0) return ino;

    if ((ret = mnt->ops->mklink(mnt, ino, mode | S_IFLNK, dino, base)) < 0) {
        mnt->ops->rmino(mnt, ino);
        return ret;
    }

    return 0;
}

int chdir(const char* path) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;
    memcpy(cwd, abs, strlen(abs) + 1);
    return 0;
}

int getcwd(char* buf, usize len) {
    if (strlen(cwd) + 1 > len) return -ETOOSMALL;
    memcpy(buf, cwd, strlen(cwd) + 1);
    return 0;
}

int canonicalize(const char* path, char* out, usize outlen) {
    return vfs_abspath(path, out, outlen);
}