//#include <ff16/ff.h>
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

int fs_probe_mount(void) {
    // we are going to start removing fat support
    if (_ext2_mount("/") < 0) {
        printf("Failed to mount EXT2\n");
        return -1;
    }
    /*if (ext2_detect()) {
        if (ext2_mount("") == 0) {
            fs_backend = FS_BACKEND_EXT2;
            return 0;
        }
        printf("EXT2 detected but refused to mount, trying FAT\n");
    }
    if (mount("", MNT_FORMAT) == 0) {
        fs_backend = FS_BACKEND_FAT;
        return 0;
    }*/
    return 0;
}

/*FATFS fs;
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
}*/

int umount(const char* path) {
    (void)path;
    return _ext2_unmount();
    //return (f_unmount(path) == FR_OK) ? 0 : -1; 
}

int open(const char* path, int flags, int mode) {
    if (flags & O_CREAT) {
        mode |= S_IFREG;
    }

    return _ext2_open(path, flags, mode);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_open(path, flags);
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
    return fd->fd;*/
}

off_t lseek(int fd, off_t off, int whence) {
    return _ext2_lseek(fd, off, whence);
    //if (fs_backend == FS_BACKEND_EXT2) return ext2_lseek(fd, off, whence);
    /*struct fdinfo* info;
    if (getfd(fd, &info) < 0) return -1;

    if (whence == SEEK_SET) {
        return f_lseek(&info->data.file, off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_CUR) {
        return f_lseek(&info->data.file, f_tell(&info->data.file) + off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_END) {
        return f_lseek(&info->data.file, f_size(&info->data.file) + off) == FR_OK ? 0 : -1;
    } else return -1;*/
}

// read-only backend has nothing to truncate
int trunc(int fd) {
    return _ext2_trunc(fd);
    /*if (fs_backend == FS_BACKEND_EXT2) return -1;
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    } 
    return f_truncate(&info->data.file) == FR_OK ? 0 : -1;*/
}

int sync(int fd) {
    return _ext2_sync(fd);
    /*if (fs_backend == FS_BACKEND_EXT2) return 0; // nothing dirty on a ro fs
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    } 
    return f_sync(&info->data.file) == FR_OK ? 0 : -1;*/
}

int opendir(const char* path) {
    return _ext2_opendir(path);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_opendir(path);
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

    return -1;*/
}

int closedir(int cdp) {
    return _ext2_close(cdp);
    //return close(cdp);
}

void convstat(FILINFO* finfo, struct stat* st) {
    memcpy(st->st_name, finfo->fname, sizeof(st->st_name));
    st->st_attrib = finfo->fattrib;
    st->st_size = finfo->fsize;
    st->mode = S_IFREG | 0x0777;
}

int readdir(int cdp, struct stat* st) {
    return _ext2_readdir(cdp, st);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_readdir(cdp, st);
    struct fdinfo* info;
    if (getfd(cdp, &info) < 0) return -1;

    FILINFO inf;
    if (f_readdir(&info->data.dir, &inf) != FR_OK) return -1;

    if (inf.fname[0] == 0) return -1;

    convstat(&inf, st);
    return 0;*/
}

int stat(const char* path, struct stat* st) {
    return _ext2_stat(path, st);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_stat(path, st);
    FILINFO inf;
    if (f_stat(path, &inf) != FR_OK) return -1;
    convstat(&inf, st);
    return 0;*/
}

// mutating ops dont exist on the read-only backend
int unlink(const char* path) {
    return _ext2_unlink(path);
    /*if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_unlink(path) == FR_OK ? 0 : -1;*/
}

int rmdir(const char* path) {
    return _ext2_rmdir(path);
}

int rename(const char* oname, const char* nname) {
    return _ext2_rename(oname, nname);
    /*if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_rename(oname, nname) == FR_OK ? 0 : -1;*/
}

int mkdir(const char* path, int mode) {
    return _ext2_mkdir(path, mode);
    /*if (fs_backend == FS_BACKEND_EXT2) return -1;
    return f_mkdir(path) == FR_OK ? 0 : -1;*/
}

int chdir(const char* path) {
    return _ext2_chdir(path);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_chdir(path);
    return f_chdir(path) == FR_OK ? 0 : -1;*/
}
int getcwd(char* buf, usize len) {
    return _ext2_getcwd(buf, len);
    /*if (fs_backend == FS_BACKEND_EXT2) return ext2_getcwd(buf, len);
    return f_getcwd(buf, len) == FR_OK ? 0 : -1;*/
}

int creat(const char* path, int mode) {
    return _ext2_creat(path, mode | S_IFREG);
}