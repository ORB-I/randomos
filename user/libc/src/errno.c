#include <errno.h>
#include <printf.h>
#include <str.h>

int __libc_errno = 0;

int* __errno_location(void) {
    return &__libc_errno;
}

int get_errno(void) {
    return __libc_errno;
}

void set_errno(int err) {
    __libc_errno = err;
}

const char* strerror(int errnum) {
    switch (errnum) {
        case EOK:          return "Success";
        case EUNKNOWN:      return "Unknown error";
        case EINVAL:       return "Invalid argument";
        case ENOMEM:       return "Out of memory";
        case EFAULT:       return "Bad address";
        case EBADF:        return "Bad file descriptor";
        case EDISK:        return "Disk error";
        case EASSERT:      return "Assertion failure";
        case ENOTRDY:      return "Device not ready";
        case ENOENT:       return "No such file or directory";
        case EACCESS:      return "Permission denied";
        case ERO:          return "Read-only file system";
        case EABORT:       return "Aborted";
        case ELOCK:        return "Lock failed";
        case ETIME:        return "Connection timed out";
        case ETOOMANYF:    return "Too many open files";
        case ENOCORE:      return "No core";
        case ENOPROC:      return "No such process";
        case EFULL:        return "Buffer full";
        case EBADEXE:      return "Exec format error";
        case ERANGE:       return "Result too large";
        case ENOEXIST:     return "Entity does not exist";
        case EHANG:        return "Operation would hang";
        case ETOOSMALL:    return "Buffer too small";
        case ENOSPC:       return "No space left on device";
        case ENOTDIR:      return "Not a directory";
        case EISDIR:       return "Is a directory";
        case ENOTEMPTY:    return "Directory not empty";
        case EEXISTS:      return "File exists";
        case ELOOP:        return "Too many symbolic links";
        case ENAMETOOLONG: return "File name too long";
        case ENODEV:       return "No such device";
        case ENOTTY:       return "Inappropriate ioctl for device";
        case EPIPE:        return "Broken pipe";
        case EINTR:        return "Interrupted system call";
        case EAGAIN:       return "Resource temporarily unavailable";
        default:           return "Unknown error code";
    }
}

void perror(const char* s) {
    const char* msg = strerror(__libc_errno);
    if (s && *s) {
        printf("%s: %s\n", s, msg);
    } else {
        printf("%s\n", msg);
    }
}
