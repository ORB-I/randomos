#include <io.h>
#include <sys/sysfn.h>

int main() {
    printf("Welcome to RandomOS\n");
    char* shargv[] = {"sh"};
    int pid = 0;
    if ((pid = newproc("sh", shargv)) < 0) {
        printf("failed to start shell\n");
    }
    wait(pid);
    for (;;);
}
