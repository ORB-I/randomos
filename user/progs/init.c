#include <io.h>
#include <sys/process.h>

int main() {
    printf("Welcome to RandomOS\n");
    char* shargv[] = {"/bin/sh", NULL};
    char* shenvp[] = {"PATH=/bin:/sbin", NULL};
    int pid = 0;
    if ((pid = newproc("/bin/sh", shargv, shenvp)) < 0) {
        printf("failed to start shell\n");
        for (;;);
    }
    int exit;
    wait(pid, &exit);
    for (;;);
}