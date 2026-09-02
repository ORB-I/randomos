#include <core/std.h>
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

static vfs_t* mounts = NULL;
char cwd[1024];
usize avmounts = 0;

#define FSFLAG_NOBLK 0x01
struct av_vfs {
    const char* id;
    u32 flags;
    int (*mount)(vfs_t* vfs);
};

struct av_vfs availfs[] = {
    {"ext2", 0, ext2fs_mount},
    {"ramfs", FSFLAG_NOBLK, ramfs_mount}
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
        serial_printf("Found mount %p at %s\n", m, m->path);

        usize len = strlen(m->path);
        if (len < mntlen) continue;
        if (strncmp(path, m->path, len) != 0) continue;

        if (path[len] == '/') continue;

        mnt = m;
        mntlen = len;
    }

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

int vfs_findino(vfs_t* mnt, const char* path, u64* ino) {
    ssize fino = mnt->root_ino;
    if (!path || *path == '\0' || !ino) {
        return -EINVAL;
    }

    path += strlen(mnt->path);
    if (*path == '\0') {
        *ino = fino;
        return 0;
    }

    if (path[0] == '/') {
        path++;
        if (*path == '\0') {
            *ino = fino;
            return 0;
        }
    }

    while (*path) {
        char comp[1024];
        usize len = 0;
        while (*path == '/') path++;
        if (*path == '\0') break;

        while (path[len] != '/' && path[len] != '\0') {
            if (len >= sizeof(comp) - 1) {
                return -EINVAL;
            }
            comp[len] = path[len];
            len++;
        }

        comp[len] = '\0';
        path += len;

        if (streq(comp, ".")) continue;
        if (streq(comp, "..")) {
            fino = mnt->ops->lookup(mnt, fino, "..");
            if (fino < 0) return fino;
            continue;
        }

        
        if ((fino = mnt->ops->lookup(mnt, fino, comp)) < 0) {
            return fino;
        }
    }

    *ino = fino;
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

    u64 dino = 0;
    if ((ret = vfs_findino(mnt, dir, &dino)) < 0) return ret;

    u64 _fino = 0;
    if ((ret = vfs_findino(mnt, abs, &_fino)) >= 0) return -EEXISTS;

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

    u64 dino = 0;
    if ((ret = vfs_findino(mnt, dir, &dino)) < 0) return ret;

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

    u64 ino = 0;
    ret = vfs_findino(mnt, abs, &ino);
    
    if (ret == -ENOENT) {
        if (flags & O_CREAT) {
            if ((ret = vfs_basecreat(abs, mode | S_IFREG, &ino)) < 0) {
                return ret;
            }
        } else {
            return ret;
        }
    } else if (ret < 0) {
        return ret;
    }

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    struct fdinfo info = {
        0, 0, FDTYPE_FILE, {.file = {
            mnt, inod, ino, 0, {0}
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

    u64 ino = 0;
    if ((ret = vfs_findino(mnt, abs, &ino)) < 0) return ret;

    vinode_t inod;
    if ((ret = mnt->ops->getino(mnt, ino, &inod)) < 0) return ret;

    if (S_TYPE(inod.mode) != S_IFDIR) {
        return -ENOTDIR;
    }

    struct fdinfo info = {
        0, 0, FDTYPE_DIR, {.dir = {
            mnt, inod, ino, 0, {0}
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
    if ((ret = getfd(dd, &fdinfo)) < 0) return ret;

    if (fdinfo->type != FDTYPE_DIR) return -ENOTDIR;
    struct file* ent = &fdinfo->data.dir;

    vinode_t inod;
    char name[1024];
    ssize fsino = 0;
    if ((fsino = ent->mnt->ops->readdir(ent->mnt, ent->ino, &ent->pos, name, 1024, &inod)) < 0) {
        return fsino;
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

int stat(const char* path, struct stat* st) {
    int ret = 0;
    char abs[1024];
    if ((ret = vfs_abspath(path, abs, 1024)) < 0) return ret;

    char base[1024];
    if ((ret = vfs_basename(abs, base, 1024)) < 0) return -1;

    vfs_t* mnt = vfs_getmnt(abs);

    u64 ino = 0;
    if ((ret = vfs_findino(mnt, abs, &ino)) < 0) return ret;

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

    u64 ino = 0;
    if ((ret = vfs_findino(mnt, abs, &ino)) < 0) return ret;

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

    u64 ino = 0;
    if ((ret = vfs_findino(mnt, abs, &ino)) < 0) return ret;

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

    u64 ino = 0;
    if ((ret = vfs_findino(omnt, oabs, &ino)) < 0) return ret;

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