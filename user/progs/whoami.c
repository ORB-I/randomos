#include <io.h>
#include <sys/process.h>

int main() {
    uid_t euid = geteuid();
    if (euid == 0) {
        printf("root\n");
    } else {
        printf("user %u\n", (u32)euid);
    }
    return 0;
}
