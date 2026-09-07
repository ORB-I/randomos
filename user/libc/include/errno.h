#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define EOK          0  // Success
#define EUNKNOWN     1  // Unknown error
#define EINVAL       2  // Invalid argument
#define ENOMEM       3  // Out of memory
#define EFAULT       4  // Bad address
#define EBADF        5  // Bad file descriptor
#define EDISK        6  // Disk error
#define EASSERT      7  // Assertion failure
#define ENOTRDY      8  // Device not ready
#define ENOENT       9  // No such file or directory
#define EACCESS      10 // Permission denied
#define EACCES       10 // POSIX alias
#define ERO          11 // Read-only file system
#define EROFS        11 // POSIX alias
#define EABORT       13 // Aborted
#define ELOCK        14 // Lock failed
#define ETIME        15 // Timeout
#define ETIMEDOUT    15 // POSIX alias
#define ETOOMANYF    16 // Too many open files
#define EMFILE       16 // POSIX alias
#define ENOCORE      17 // No core
#define ENOPROC      18 // No such process
#define ESRCH        18 // POSIX alias
#define EFULL        19 // Buffer full
#define EBADEXE      20 // Exec format error
#define ENOEXEC      20 // POSIX alias
#define ERANGE       21 // Result too large
#define ENOEXIST     22 // Entity does not exist
#define EHANG        23 // Operation would hang
#define ETOOSMALL    24 // Buffer too small
#define ENOSPC       25 // No space left on device
#define ENOTDIR      26 // Not a directory
#define EISDIR       27 // Is a directory
#define ENOTEMPTY    28 // Directory not empty
#define EEXISTS      29 // File exists
#define EEXIST       29 // POSIX alias
#define ELOOP        30 // looped too much
#define ENAMETOOLONG 31 // name too long
#define ENODEV       32 // no such device
#define ENOTTY       33 // inappropriate ioctl for device
#define EPIPE        34 // broken pipe
#define EINTR        35 // interrupted syscall
#define EAGAIN       36 // resource temporarily unavailable
#define EWOULDBLOCK  36 // operation would block

int* __errno_location(void);
#define errno (*__errno_location())

int get_errno(void);
void set_errno(int err);
const char* strerror(int errnum);
void perror(const char* s);


#ifdef __cplusplus
}
#endif
