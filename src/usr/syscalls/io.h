#pragma once
#include "ssc.h"

DEFSYSCALL(sys_termctl);
DEFSYSCALL(sys_createfb);
DEFSYSCALL(sys_switchfb);
DEFSYSCALL(sys_clearfb);
DEFSYSCALL(sys_flushscr);
DEFSYSCALL(sys_getfbinf);
DEFSYSCALL(sys_getcurfb);
DEFSYSCALL(sys_getrawsc);
DEFSYSCALL(sys_createfbwmem);
DEFSYSCALL(sys_getmouseinfo);
DEFSYSCALL(sys_serialwrite);
DEFSYSCALL(sys_getrawscto);
DEFSYSCALL(sys_setcurs);
DEFSYSCALL(sys_getcurs);