#include <ff16/ff.h>
#include <core/std.h>
#include <core/printf.h>
#include <lib/string.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/ext2.h>
#include <drivers/display/serial.h>
#include <drivers/display/term.h>
#include <drivers/hid/kbd.h>
#include <core/fd.h>

int fs_backend = FS_BACKEND_FAT;

/* figure out what's actually on the drive before anything gets mounted.
   ext2 wins if its superblock checks out, fat is the fallback. ordering
   matters here: mount() with MNT_FORMAT would happily format over a
   filesystem we just failed to recognize */
int fs_probe_mount(void) {
    if (ext2_detect()) {
        if (ext2_mount("") == 0) {
            fs_backend = FS_BACKEND_EXT2;
            return 0;
        }
        printf("EXT2 detected but refused to mount, trying FAT\n");
    }
    if (mount("", MNT_FORMAT) == 0) {
        fs_backend = FS_BACKEND_FAT;
        return 0;
    }
    return -1;
}

FATFS fs;
int mount(const char* path, int flags) {
    FRESULT res = f_mount(&fs, path, 1);
    if (res == FR_OK) {
        return f_chdrive(path) == FR_OK ? 0 : -1;
    } else if (res == FR_NO_FILESYSTEM && ((flags & MNT_FORMAT) == MNT_FORMAT)) {
        printf("creating fs\n");
        u8 work[FF_MAX_SS];
        res = f_mkfs(path, NULL, work, sizeof(work));
        if (res == FR_OK) {
            if (f_mount(&fs, path, 1) == FR_OK) {
                return f_chdrive(path) == FR_OK ? 0 : -1;
            } else return -1;
        } else return -1;
    } else return -1;
}

int umount(const char* path) { return (f_unmount(path) == FR_OK) ? 0 : -1; }

int open(const char* path, int flags) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_open(path, flags);
    struct fdinfo fdi = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_FILE,
    };
    struct fdinfo* fd = getnewfd(&fdi);
    if (!fd) {
        return -1;
    }

    FIL fp;
    FRESULT res = f_open(&fp, path, flags & ~O_TRUNC);
    if (res != FR_OK) {
        closefd(fd->fd);
        return -1;
    }
    if ((flags & O_TRUNC) == O_TRUNC) {
        res = f_truncate(&fp);
        if (res != FR_OK) {
            closefd(fd->fd);
            f_close(&fp);
            return -1;
        }
    }
    fd->data.file = fp;
    return fd->fd;
}

off_t lseek(int fd, off_t off, int whence) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_lseek(fd, off, whence);
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) return -1;

    if (whence == SEEK_SET) {
        return f_lseek(&info->data.file, off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_CUR) {
        return f_lseek(&info->data.file, f_tell(&info->data.file) + off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_END) {
        return f_lseek(&info->data.file, f_size(&info->data.file) + off) == FR_OK ? 0 : -1;
    } else return -1;
}

// read-only backend has nothing to truncate
int trunc(int fd) {
    if (fs_backend == FS_BACKEND_EXT2) return -1;
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    } 
    return f_truncate(&info->data.file) == FR_OK ? 0 : -1; 
}

int sync(int fd) {
    if (fs_backend == FS_BACKEND_EXT2) return 0; // nothing dirty on a ro fs
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    } 
    return f_sync(&info->data.file) == FR_OK ? 0 : -1; 
}

int opendir(const char* path) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_opendir(path);
    struct fdinfo fdi = {
        .fd = -1,
        .inuse = 0,
        .type = FDTYPE_DIR,
    };
    struct fdinfo* fd = getnewfd(&fdi);
    if (!fd) {
        return -1;
    }

    if (f_opendir(&fd->data.dir, path) == FR_OK) {
        return fd->fd;
    } else {
        closefd(fd->fd);
    }

    return -1;
}

int closedir(int cdp) {
    return close(cdp);
}

void convstat(FILINFO* finfo, struct stat* st) {
    memcpy(st->st_name, finfo->fname, sizeof(st->st_name));
    st->st_attrib = finfo->fattrib;
    st->st_size = finfo->fsize;
}

int readdir(int cdp, struct stat* st) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_readdir(cdp, st);
    struct fdinfo* info;
    if (getfd(cdp, &info) < 0) return -1;

    FILINFO inf;
    if (f_readdir(&info->data.dir, &inf) != FR_OK) return -1;

    if (inf.fname[0] == 0) return -1;

    convstat(&inf, st);
    return 0;
}

int stat(const char* path, struct stat* st) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_stat(path, st);
    FILINFO inf;
    if (f_stat(path, &inf) != FR_OK) return -1;
    convstat(&inf, st);
    return 0;
}

// mutating ops dont exist on the read-only backend
int unlink(const char* path) {
    if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_unlink(path) == FR_OK ? 0 : -1;
}
int rename(const char* oname, const char* nname) {
    if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_rename(oname, nname) == FR_OK ? 0 : -1;
}
int mkdir(const char* path) {
    if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_mkdir(path) == FR_OK ? 0 : -1;
}
int chdir(const char* path) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_chdir(path);
    return f_chdir(path) == FR_OK ? 0 : -1;
}
int getcwd(char* buf, usize len) {
    if (fs_backend == FS_BACKEND_EXT2) return ext2_getcwd(buf, len);
    return f_getcwd(buf, len) == FR_OK ? 0 : -1;
}