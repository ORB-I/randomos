#include <sys/sysfn.h>

int main() {
    if (reboot() < 0) {
        return 1;
    }
    return 0;
}