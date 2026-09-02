#include <lib/string.h>
#include <core/fd.h>
#include <drivers/storage/fs.h>
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
        case FDTYPE_FILE: 
        case FDTYPE_DIR: return closefd(fd);
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
        case FDTYPE_FILE: return fsread(fd, buf, size);
        case FDTYPE_DIR: return -EINVAL;
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
        case FDTYPE_FILE: return fswrite(fd, buf, size);
        case FDTYPE_DIR: return -EINVAL;
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