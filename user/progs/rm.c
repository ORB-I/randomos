#include <fs.h>
#include <io.h>

int main(int ac, char** av) {
    if (ac < 2) {
        printf("not enough arguments\n");
        return 1;
    }

    // dont tell them but
    // inside the kernel unlink and rmdir
    // are defined the exact same way
    // just with different numbers
    if (unlink(av[1]) < 0) {
        printf("failed to remove file or directory\n");
        return 1;
    }

    return 0;
}