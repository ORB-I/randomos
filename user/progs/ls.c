#include <fs.h>
#include <io.h>
#include <str.h>
#include <sys/syscall.h>
#include <sys/sysfn.h>

void _putchar(char c) {
    __syscall3(SYS_WRITE, STDOUT, (u64)&c, 1);
}

int list_dir(char* path) {
    DIR* d = opendir(path);
    if (!d) {
        printf("Failed to open: %s\n", path);
        return 1;
    }

    struct stat st;
    while ((readdir(d, &st)) != -1) {
        putchar('\t');
        const char* name = st.st_name;
        while (*name != '\0') _putchar(*name++);
        putchar('\n');
    }

    closedir(d);
    return 0;
}

int main(int ac, char** av) {
    if (ac < 2) {
        return list_dir(".");
    } else if (ac < 3) {
        return list_dir(av[1]);
    } else {
        int rc = 0;
        for (int i = 1; i < ac; i++) {
            printf("%s:\n", av[i]);
            int ret = list_dir(av[i]);
            if (ret == 1) rc = 1;
        }
        return rc;
    }
}