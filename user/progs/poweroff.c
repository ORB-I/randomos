#include <sys/sysfn.h>

int main() {
    if (poweroff() < 0) {
        return 1;
    }
    return 0;
}