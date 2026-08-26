#pragma once
#include "ssc.h"

DEFSYSCALL(sys_wait);
DEFSYSCALL(sys_exit);
DEFSYSCALL(sys_kill);
DEFSYSCALL(sys_newproc);
DEFSYSCALL(sys_execve);
DEFSYSCALL(sys_getpid);
DEFSYSCALL(sys_getuid);
DEFSYSCALL(sys_setuid);
DEFSYSCALL(sys_getgid);
DEFSYSCALL(sys_setgid);
DEFSYSCALL(sys_geteuid);
DEFSYSCALL(sys_seteuid);
DEFSYSCALL(sys_getegid);
DEFSYSCALL(sys_setegid);