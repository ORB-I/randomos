#include <io.h>
#include <sys/sysfn.h>

void fputchar(int fd, char c) {
    write(fd, &c, 1);
}

void putchar(char  c) {
    write(STDOUT, &c, 1);
}