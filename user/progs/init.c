#include <io.h>
#include <sys/process.h>

int main() {
    printf("Welcome to RandomOS\n");
    char* shargv[] = {"/bin/login", NULL};
    char* shenvp[] = {"PATH=/bin:/sbin", NULL};
    int pid = 0;

    while (1) {
        if ((pid = newproc("/bin/login", shargv, shenvp)) < 0) {
            printf("failed to start login\n");
            for (;;);
        }
        int exit;
        wait(pid, &exit);
        printf("login exited with code %d\n", exit);
    }
    for (;;);
}