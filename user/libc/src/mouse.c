#include <mouse.h>
#include <sys/syscall.h>

int get_mouse_info(mouse_info_t* buf) {
    return __syscall1(SYS_GETMOUSEINFO, (u64)buf);
}