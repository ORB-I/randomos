#include <sys/sysfn.h>
#include <sys/ioctl.h>

int main() {
    if (ioctl(STDOUT, TCTL_CLEAR, 0) < 0) {
        return 1;
    }
    return 0;
}