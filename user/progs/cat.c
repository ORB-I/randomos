#include <io.h>
#include <fs.h>
#include <sys/sysfn.h>

#define CAT_CHUNK 512

int main(int ac, char** av) {
    if (ac < 2) {
        printf("usage: cat <file>\n");
        return 1;
    }

    int fd = open(av[1], O_RDONLY, 0);
    if (fd < 0) {
        printf("cannot open %s\n", av[1]);
        return 1;
    }

    char buf[CAT_CHUNK];
    ssize n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    close(fd);
    return n < 0 ? 1 : 0;
}
