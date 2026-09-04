#pragma once
#include "ssc.h"

DEFSYSCALL(sys_reboot);
DEFSYSCALL(sys_poweroff);
DEFSYSCALL(sys_mmap);
DEFSYSCALL(sys_munmap);
DEFSYSCALL(sys_random64);
DEFSYSCALL(sys_randombytes);
DEFSYSCALL(sys_mprotect);