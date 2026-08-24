#include <fs.h>
#include <io.h>
#include <str.h>
#include <sys/syscall.h>
#include <sys/sysfn.h>

int list_dir(char* path) {
    int d = opendir(path);
    if (!d) {
        printf("Failed to open: %s\n", path);
        return 1;
    }

    struct stat st;
    termctl(TCTL_AFLSH, 0);
    while ((readdir(d, &st)) != -1) {
        printf("\t%s\n", st.st_name);
    }
    termctl(TCTL_AFLSH, 1);
    termctl(TCTL_FLUSH, 0);

    close(d);
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