#include <sys/sysfn.h>
#include <io.h>

int list_dir(char* path) {
    DIR* d = opendir(path);
    if (!d) {
        printf("Failed to open: %s\n", path);
        return 1;
    }

    struct stat st;
    while ((readdir(d, &st)) != -1) {
        printf("\t%s\n", st.st_name);
    }

    closedir(d);
    return 0;
}

int main(int ac, char** av) {
    printf("ac: %d\n", ac);
    printf("av: \n");
    for (int i = 0; i < ac; i++) printf("    %s\n", av[i]);
    
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