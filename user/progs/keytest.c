#include <kbd.h>
#include <io.h>

int main() {
    while (1) {
        u8 sc = kbd_get_raw();
        printf("Scancode: %x\n", sc);
        if (sc == 0x01) {
            break;
        }
    }
    return 0;
}