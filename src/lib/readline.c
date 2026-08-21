#include <core/liballoc.h>
#include <drivers/term.h>
#include <core/printf.h>
#include <drivers/hid/kbd.h>

#define INITBUFSZ 256

char* readline(const char* prompt) {
    u32 bufsz = INITBUFSZ;
    char* buf = (char*)malloc(bufsz);

    if (!buf) return NULL;
    if (prompt != NULL) {
        printf("%s", prompt);
        term_flush();
    }

    usize i = 0;
    noecho(1);

    while (1) {
        char c = (char)getchar();
        if (c == '\n' || c == '\r') {
            term_putchar(c);
            break;
        } else if (c == '\b') {
            if (i == 0) continue;
            else {
                buf[i--] = '\0';
                term_putchar('\b');
                term_putchar(' ');
                term_putchar('\b');
                continue;
            }
        } else {
            term_putchar(c);
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
    noecho(0);

    buf[i] = '\0';
    return buf;
}
