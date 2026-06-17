#include <io.h>

int main(int ac, char** av) {
    for (int i = 1; i < ac; i++) {
        printf("%s", av[i]);
    }
    putchar('\n');
    return 0;
}