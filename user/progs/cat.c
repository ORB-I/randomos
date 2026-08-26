#include <io.h>
#include <fs.h>
#include <sys/sysfn.h>

#define CAT_CHUNK 512

int main(int ac, char** av) {
    if (ac < 2) {
        printf("usage: cat <file>\n");
        return 1;
    }

    int fd = open(av[1], O_RDONLY);
    if (fd < 0) {
        printf("cannot open %s\n", av[1]);
        return 1;
    }

    char buf[CAT_CHUNK];
    ssize n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize i = 0; i < n; i++) putchar(buf[i]);
    }
    close(fd);
    return n < 0 ? 1 : 0;
}
