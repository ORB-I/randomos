#include <sys/sysfn.h>

int main() {
    if (termctl(TCTL_CLEAR, 0) < 0) {
        return 1;
    }
    return 0;
}