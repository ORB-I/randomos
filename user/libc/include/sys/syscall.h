#pragma once
#include <sys/types.h>

#define SYS_EXIT     1
#define SYS_READ     2
#define SYS_WRITE    3
#define SYS_OPEN     4
#define SYS_CLOSE    5
#define SYS_CREAT    6
#define SYS_UNLINK   7
#define SYS_CHDIR    8
#define SYS_LSEEK    9
#define SYS_RENAME   10
#define SYS_MKDIR    11
#define SYS_RMDIR    12
#define SYS_REBOOT   13
#define SYS_STAT     14
#define SYS_POWEROFF 15
#define SYS_SLEEP    16
#define SYS_READDIR  17
#define SYS_OPENDIR  18
#define SYS_CLOSEDIR 19
#define SYS_GETCWD   20
#define SYS_SYNC     21
#define SYS_TRUNC    22

u32 __syscall0(u32 nr);
u32 __syscall1(u32 nr, u32 arg0);
u32 __syscall2(u32 nr, u32 arg0, u32 arg1);
u32 __syscall3(u32 nr, u32 arg0, u32 arg1, u32 arg2);
u32 __syscall4(u32 nr, u32 arg0, u32 arg1, u32 arg2, u32 arg3);
u32 __syscall5(u32 nr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4);