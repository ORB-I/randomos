#pragma once

#define EOK       0  // ok
#define EUNKNOWN  1  // unknown error
#define EINVAL    2  // invalid argument or state
#define ENOMEM    3  // no memory left
#define EFAULT    4  // bad pointer
#define EBADF     5  // bad fd
#define EDISK     6  // disk error
#define EASSERT   7  // assertation failure
#define ENOTRDY   8  // not ready
#define ENOENT    9  // no file or directory
#define EACCESS   10 // access denied
#define ERO       11 // read-only
#define EABORT    13 // abort
#define ELOCK     14 // lock failed
#define ETIME     15 // timeout
#define ETOOMANYF 16 // too many files open
#define ENOCORE   17 // core doesnt exist
#define ENOPROC   18 // no processes left or process doesnt exist
#define EFULL     19 // buffer is full
#define EBADEXE   20 // malformed or unsupported executable
#define ERANGE    21 // out of range
#define ENOEXIST  22 // something doesnt exist
#define EHANG     23 // will hang
#define ETOOSMALL 24 // buffer too small
#define ENOSPC    25 // no space on disk
#define ENOTDIR   26 // not a directory
#define EISDIR    27 // is a directory
#define ENOTEMPTY 28 // directory not empty
#define EEXISTS   29 // file or directory already exists

#define FF_TO_ERRNO(FF)  \
    (FF == FR_OK ? EOK : \
     FF == FR_DISK_ERR ? EDISK : \
     FF == FR_INT_ERR ? EASSERT : \
     FF == FR_NOT_READY ? ENOTRDY : \
     FF == FR_NO_FILE ? ENOENT : \
     FF == FR_NO_PATH ? ENOENT : \
     FF == FR_INVALID_NAME ? EINVAL : \
     FF == FR_DENIED ? EACCESS : \
     FF == FR_EXIST ? EACCESS : \
     FF == FR_INVALID_OBJECT ? EINVAL : \
     FF == FR_WRITE_PROTECTED ? ERO : \
     FF == FR_INVALID_DRIVE ? EINVAL : \
     FF == FR_NOT_ENABLED ? EINVAL : \
     FF == FR_NO_FILESYSTEM ? ENOENT : \
     FF == FR_MKFS_ABORTED ? EABORT : \
     FF == FR_TIMEOUT ? ETIME : \
     FF == FR_LOCKED ? ELOCK : \
     FF == FR_NOT_ENOUGH_CORE ? ENOMEM : \
     FF == FR_TOO_MANY_OPEN_FILES ? ETOOMANYF : \
     FF == FR_INVALID_PARAMETER ? EINVAL : \
     EUNKNOWN \
    )