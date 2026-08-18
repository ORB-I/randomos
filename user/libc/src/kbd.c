#include <kbd.h>
#include <sys/syscall.h>

u8 kbd_get_raw(void) {
    return (u8)__syscall0(SYS_GETRAWSC);
}