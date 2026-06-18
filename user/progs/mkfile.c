#include <fs.h>
#include <io.h>

int main(int ac, char** av) {
    if (ac < 2) {
        printf("not enough arguments\n");
        return 1;
    }

    if (creat(av[1]) < 0) {
        printf("failed to create file\n");
        return 1;
    }
    
    return 0;
}