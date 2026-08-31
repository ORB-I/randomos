#include <ff16/ff.h>
#include <lib/string.h>
#include <core/fd.h>
#include <drivers/storage/fs.h>
#include <drivers/storage/ext2.h>
#include <scheduler/process.h>
#include <core/liballoc.h>
#include <drivers/display/fb.h>
#include <core/errno.h>

struct fdinfo* getnewfd(struct fdinfo* info) {
    struct fdinfo* fds = proctbl[current_pid].fds;
    usize nfds = proctbl[current_pid].nfds;

    for (usize i = 0; i < nfds; i++) {
        if (!fds[i].inuse) {
            memcpy(&fds[i], info, sizeof(*info));
            fds[i].inuse = 1;
            fds[i].fd = i;
            return &fds[i];
        }
    }

    void* newptr = realloc(fds, sizeof(struct fdinfo) * (nfds + 10));
    if (!newptr) return NULL;
    fds = newptr;

    proctbl[current_pid].fds = newptr;
    proctbl[current_pid].nfds += 10;

    memcpy(&fds[nfds], info, sizeof(*info));
    fds[nfds].inuse = 1;
    fds[nfds].fd = nfds;
    return &fds[nfds];
}

int getfd(int fd, struct fdinfo** info) {
    if (fd >= (int)proctbl[current_pid].nfds) return -EBADF;
    if (fd < 0) return -EBADF;
    if (!proctbl[current_pid].fds[fd].inuse) return -EBADF;

    *info = &proctbl[current_pid].fds[fd];
    return 0;
}

int closefd(int fd) {
    struct fdinfo* fds = proctbl[current_pid].fds;
    usize nfds = proctbl[current_pid].nfds;

    if (fd >= (int)nfds) return -EBADF;
    if (fd < 0) return -EBADF;

    fds[fd].inuse = 0;
    return 0;
}

int close(int fd) {
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            // ext2 keeps no kernel-side handle to close
            return _ext2_close(fd);
            /*if (fs_backend == FS_BACKEND_EXT2) {
                break;
            }
            if ((ret = f_close(&info->data.file)) != FR_OK) {
                return -FF_TO_ERRNO(ret);
            }*/
            break;
        }
        case FDTYPE_DIR: {
            if (fs_backend == FS_BACKEND_EXT2) {
                break;
            }
            if ((ret = f_closedir(&info->data.dir)) != FR_OK) {
                return -FF_TO_ERRNO(ret);
            }
            break;
        }
        case FDTYPE_SOCK: return -EINVAL;
        case FDTYPE_FB: {
            return free_fb(fd);
        }
        case FDTYPE_FBW: {
            return free_fb_withmem(fd);
        }
        case FDTYPE_IO: {
            break;
        }
    }

    return closefd(fd);
}

ssize read(int fd, void* buf, usize size) {
    struct fdinfo* info;
    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            return _ext2_read(fd, buf, size);
            /* ext2 fds share the FILE type but keep their own state
            if (fs_backend == FS_BACKEND_EXT2) {
                return ext2_read(fd, buf, size);
            }

            UINT nread;
            if ((ret = f_read(&info->data.file, buf, size, &nread)) == FR_OK) {
                return (ssize)nread;
            } else {
                return FF_TO_ERRNO(ret);
            }*/
        }
        case FDTYPE_DIR:
        case FDTYPE_FB: 
        case FDTYPE_FBW: {
            return -EINVAL;
        }
        case FDTYPE_SOCK: {
            return -EINVAL;
        }
        case FDTYPE_IO: {
            if (info->data.io.in) {
                return info->data.io.read(buf, size);
            } else {
                return -EBADF;
            }
        }
        default: return -EINVAL;
    }
}

ssize write(int fd, void* buf, usize size) {
    struct fdinfo* info;

    int ret = 0;
    if ((ret = getfd(fd, &info)) < 0) {
        return ret;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            return _ext2_write(fd, buf, size);
            /* read-only backend, nothing to write
            if (fs_backend == FS_BACKEND_EXT2) {
                return -ERO;
            }
            UINT nwritten;
            return (f_write(&info->data.file, buf, size, &nwritten) == FR_OK ? (ssize)nwritten : -1);*/
        }
        case FDTYPE_DIR:
        case FDTYPE_FB:
        case FDTYPE_FBW: {
            return -EINVAL;
        }
        case FDTYPE_SOCK: {
            return -EINVAL;
        }
        case FDTYPE_IO: {
            if (info->data.io.out) {
                return info->data.io.write(buf, size);
            } else {
                return -EBADF;
            }
        }
        default: return -EINVAL;
    }
}