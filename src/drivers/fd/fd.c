#include <ff16/ff.h>
#include <lib/string.h>
#include <core/fd.h>
#include <scheduler/process.h>
#include <core/liballoc.h>
#include <drivers/display/fb.h>

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
    if (fd >= (int)proctbl[current_pid].nfds) return -1;
    if (fd < 0) return -1;

    *info = &proctbl[current_pid].fds[fd];
    return 0;
}

int closefd(int fd) {
    struct fdinfo* fds = proctbl[current_pid].fds;
    usize nfds = proctbl[current_pid].nfds;

    if (fd >= (int)nfds) return -1;
    if (fd < 0) return -1;

    fds[fd].inuse = 0;
    return 0;
}

int close(int fd) {
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            if (f_close(&info->data.file) != FR_OK) {
                return -1;
            }
            break;
        }
        case FDTYPE_DIR: {
            if (f_closedir(&info->data.dir) != FR_OK) {
                return -1;
            }
            break;
        }
        case FDTYPE_SOCK: return -1;
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
    if (getfd(fd, &info) < 0) {
        return -1;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            UINT nread;
            return (f_read(&info->data.file, buf, size, &nread) == FR_OK ? (ssize)nread : -1);
        }
        case FDTYPE_DIR:
        case FDTYPE_FB: 
        case FDTYPE_FBW: {
            return -1;
        }
        case FDTYPE_SOCK: {
            return -1;
        }
        case FDTYPE_IO: {
            if (info->data.io.in) {
                return info->data.io.read(buf, size);
            } else {
                return -1;
            }
        }
        default: return -1;
    }
}

ssize write(int fd, void* buf, usize size) {
    struct fdinfo* info;
    if (getfd(fd, &info) < 0) {
        return -1;
    }

    switch (info->type) {
        case FDTYPE_FILE: {
            UINT nwritten;
            return (f_write(&info->data.file, buf, size, &nwritten) == FR_OK ? (ssize)nwritten : -1);
        }
        case FDTYPE_DIR:
        case FDTYPE_FB:
        case FDTYPE_FBW: {
            return -1;
        }
        case FDTYPE_SOCK: {
            return -1;
        }
        case FDTYPE_IO: {
            if (info->data.io.out) {
                return info->data.io.write(buf, size);
            } else {
                return -1;
            }
        }
        default: return -1;
    }
}