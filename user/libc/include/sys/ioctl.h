#pragma once
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCTL_FLUSH    0
#define TCTL_CLEAR    1
#define TCTL_SCLR     2
#define TCTL_CCLR     3
#define TCTL_AFLSH    4
#define TCTL_GAFLH    5
#define TCTL_NOECHO   6
#define TCTL_SETCURS  7
#define TCTL_GETCURS  8

#define FB_IOCTL_GETINFO 0x4600
#define FB_IOCTL_CLEAR   0x4601
#define FB_IOCTL_FLUSH   0x4602


int ioctl(int fd, int cmd, void* data);

#ifdef __cplusplus
}
#endif
