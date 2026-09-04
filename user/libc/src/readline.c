#include <io.h>
#include <kbd.h>
#include <mem.h>
#include <sys/sysfn.h>

#define INITBUFSZ 256

extern void _flush_stdout_buffer(void);

char* readline(const char* prompt) {
    u32 bufsz = INITBUFSZ;
    char* buf = (char*)malloc(bufsz);

    if (!buf) return NULL;
    if (prompt != NULL) {
        printf("%s", prompt);
        termctl(TCTL_FLUSH, 0);
    }

    termctl(TCTL_NOECHO, 1);
    usize i = 0;

    while (1) {
        char c = (char)getchar();
        if (c == '\n' || c == '\r') {
            putchar(c);
            _flush_stdout_buffer();
            break;
        } else if (c == '\b') {
            if (i == 0) continue;
            else {
                buf[i--] = '\0';
                putchar('\b');
                putchar(' ');
                putchar('\b');
                _flush_stdout_buffer();
                continue;
            }
        } else {
            putchar(c);
            _flush_stdout_buffer();
        }

        if (i >= bufsz - 1) {
            char* newptr = (char*)realloc(buf, bufsz + 256);
            if (newptr == NULL) {
                free(buf);
                return NULL;
            }
            bufsz += 256;
            buf = newptr;
        }

        buf[i] = c;
        i++;
    }
    termctl(TCTL_NOECHO, 0);

    buf[i] = '\0';
    return buf;
}
