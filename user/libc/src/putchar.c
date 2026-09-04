#include <io.h>
#include <sys/sysfn.h>

static char stdout_buffer[256];
static usize stdout_used = 0;

void _flush_stdout_buffer(void) {
    if (stdout_used != 0) {
        write(STDOUT, stdout_buffer, stdout_used);
        stdout_used = 0;
    }
}

void fputchar(int fd, char c) {
    if (fd != STDOUT) {
        write(fd, &c, 1);
        return;
    }

    stdout_buffer[stdout_used++] = c;
    if (stdout_used == sizeof(stdout_buffer) || c == '\n') {
        _flush_stdout_buffer();
    }
}

void putchar(char  c) {
    fputchar(STDOUT, c);
}