#pragma once
#include "ssc.h"

DEFSYSCALL(sys_read);
DEFSYSCALL(sys_write);
DEFSYSCALL(sys_open);
DEFSYSCALL(sys_close);
DEFSYSCALL(sys_creat);
DEFSYSCALL(sys_unlink);
DEFSYSCALL(sys_lseek);
DEFSYSCALL(sys_rename);
DEFSYSCALL(sys_mkdir);
DEFSYSCALL(sys_rmdir);
DEFSYSCALL(sys_stat);
DEFSYSCALL(sys_readdir);
DEFSYSCALL(sys_opendir);
DEFSYSCALL(sys_sync);
DEFSYSCALL(sys_trunc);
DEFSYSCALL(sys_getpwd);
DEFSYSCALL(sys_setpwd);