#include <ff16/ff.h>
#include <core/std.h>
#include <lib/string.h>
#include <drivers/fs.h>
#include <drivers/term.h>
#include <drivers/kbd.h>

FIL fp[128];
DIR dp[128];

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
    int fd = -1;
    for (int i = 0; i < 128; i++) {
        if (fp[i].obj.fs == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) return -1;

    FRESULT res = f_open(&fp[fd], path, flags & ~O_TRUNC);
    if (res != FR_OK) return -1;
    if ((flags & O_TRUNC) == O_TRUNC) {
        res = f_truncate(&fp[fd]);
        if (res != FR_OK) return -1;
        else return fd + 3;
    }
    return fd + 3; // adding 3 to skip stdin, stdout, stderr
}

int close(int fd) {
    if (fd < 0 || fd >= 128 + 3) return -1;
    if (fd < 3) {
        return 0;
    } else {
        if (fp[fd - 3].obj.fs == NULL) return -1;

        FRESULT res = f_close(&fp[fd - 3]);
        if (res != FR_OK) return -1;
        memset(&fp[fd - 3], 0, sizeof(FIL));
    }
    return 0;
}

ssize read(int fd, void* buf, usize size) {
    if (fd < 3) {
        switch (fd) {
            // can read stdin but not stdout/stderr yet lol
            case 0:
                return getstr(buf, size);
            case 1: return -1;
            case 2: return -1;
            default: return -1; // doing this to make the compiler happy, even though it cant be reached
        }
    } else {
        UINT nread;
        return (f_read(&fp[fd - 3], buf, size, &nread) == FR_OK) ? (ssize)nread : -1;
    }
}

ssize write(int fd, void* buf, usize size) {
    if (fd < 3) {
        switch (fd) {
            case 0: return -1; // cant write to stdin yet
            case 1:
            case 2:
                for (usize i = 0; i < size; i++) term_putchar(((char*)buf)[i]);
                term_flush();
                return size;
            default: return -1;
        }
    } else {
        UINT nwritten;
        return (f_write(&fp[fd - 3], buf, size, &nwritten) == FR_OK) ? (ssize)nwritten : -1;
    }
}

off_t lseek(int fd, off_t off, int whence) {
    if (fd < 3) return -1;
    if (whence == SEEK_SET) {
        return f_lseek(&fp[fd - 3], off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_CUR) {
        return f_lseek(&fp[fd - 3], f_tell(&fp[fd - 3]) + off) == FR_OK ? 0 : -1;
    } else if (whence == SEEK_END) {
        return f_lseek(&fp[fd - 3], f_size(&fp[fd - 3]) + off) == FR_OK ? 0 : -1;
    } else return -1;
}

int trunc(int fd) { if (fd < 3) return -1; return f_truncate(&fp[fd - 3]) == FR_OK ? 0 : -1; }
int sync(int fd)  { if (fd < 3) return -1; return f_sync(&fp[fd - 3]) == FR_OK ? 0 : -1; }

DIR* opendir(const char* path) {
    for (int i = 0; i < 128; i++) {
        if (dp[i].obj.fs == NULL) {
            if (f_opendir(&dp[i], path) != FR_OK) {
                return NULL;
            }
            return &dp[i];
        }
    }
    return NULL;
}

int closedir(DIR* cdp) {
    if (!cdp) return -1;
    if (f_closedir(cdp) == FR_OK) {
        memset(cdp, 0, sizeof(DIR));
        return 0;
    } 
    return -1;
}

void convstat(FILINFO* finfo, struct stat* st) {
    memcpy(st->st_name, finfo->fname, sizeof(st->st_name));
    st->st_attrib = finfo->fattrib;
    st->st_size = finfo->fsize;
}

int readdir(DIR* cdp, struct stat* st) { 
    FILINFO inf;
    if (f_readdir(cdp, &inf) != FR_OK) return -1;
    if (inf.fname[0] == 0) return -1;

    convstat(&inf, st);
    return 0;
}

int stat(const char* path, struct stat* st) {
    FILINFO inf;
    if (f_stat(path, &inf) != FR_OK) return -1;
    convstat(&inf, st);
    return 0;
}

int unlink(const char* path) { return f_unlink(path) == FR_OK ? 0 : -1; }
int rename(const char* oname, const char* nname) { return f_rename(oname, nname) == FR_OK ? 0 : -1; }
int mkdir(const char* path) { return f_mkdir(path) == FR_OK ? 0 : -1; }
int chdir(const char* path) { return f_chdir(path) == FR_OK ? 0 : -1; }
int getcwd(char* buf, usize len) { return f_getcwd(buf, len) == FR_OK ? 0 : -1; }