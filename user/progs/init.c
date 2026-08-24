#include <io.h>
#include <sys/process.h>

int main() {
    serial_printf("Welome to RandomOS\r\n");
    printf("Welcome to RandomOS\n");
    char* shargv[] = {"/bin/login", NULL};
    char* shenvp[] = {"PATH=/bin:/sbin", NULL};
    int pid = 0;

    while (1) {
        serial_printf("Starting login process\r\n");
        if ((pid = newproc("/bin/login", shargv, shenvp)) < 0) {
            serial_printf("Login process start failed\r\n");
            printf("failed to start login\n");
            for (;;);
        }
        int exit;
        serial_printf("Waiting on login process\r\n");
        wait(pid, &exit);
    }
    for (;;);
}