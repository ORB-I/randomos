#include <kbd.h>
#include <sys/syscall.h>
#include <sys/sysfn.h>
u8 kbd_get_raw(void) {
    return (u8)__syscall0(SYS_GETRAWSC);
}

char getchar() {
    char c;
    if (read(STDIN, &c, 1) < 0) return 0;
    return c;
}