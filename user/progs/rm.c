#include <fs.h>
#include <io.h>

int main(int ac, char** av) {
    if (ac < 2) {
        printf("not enough arguments\n");
        return 1;
    }

    struct stat st;
    if (stat(av[1], &st) < 0) return 1;

    if (S_TYPE(st.st_mode) == S_IFDIR) {
        if (rmdir(av[1]) < 0) {
            printf("directory not empty\n");
            return 1;
        }
    } else {
        if (unlink(av[1]) < 0) {
            printf("failed to remove file or directory\n");
            return 1;
        }
    }

    return 0;
}