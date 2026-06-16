#include <sys/sysfn.h>
#include <str.h>
#include <io.h>

int main(int ac, char** av) {
    if (ac < 2) {
        printf("not enough args\n");
        return 1;
    }
    int n = atoi(av[1]);
    if (n < 0) {
        printf("%d\n", n);
    }
    sleep(n);
    return 0;
}