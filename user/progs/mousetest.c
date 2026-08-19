#include <mouse.h>
#include <io.h>
#include <kbd.h>

int main() {
    mouse_info_t mbuf;
    while (1) {
        if (get_mouse_info(&mbuf) < 0) {
            printf("Failed to get mouse\n");
            break;
        }

        int kbsc = kbd_get_raw();
        if (kbsc == 0x01) {
            break;
        }

        printf("Mouse: X=%d,Y=%d,LEFT=%c,RIGHT=%c,MIDDLE=%c\n",
            mbuf.x, mbuf.y,
            (mbuf.buttons & MOUSE_BUTTON_LEFT) ? 'Y' : 'N',
            (mbuf.buttons & MOUSE_BUTTON_RIGHT) ? 'Y' : 'N',
            (mbuf.buttons & MOUSE_BUTTON_MIDDLE) ? 'Y' : 'N'
        );
    }
    return 0;
}