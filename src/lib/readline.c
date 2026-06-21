#include <core/liballoc.h>

#include <drivers/term.h>
#include <drivers/kbd.h>

#define INITBUFSZ 256

char* readline(const char* prompt) {
    u32 bufsz = INITBUFSZ;
    char* buf = (char*)malloc(bufsz);

    if (!buf) return NULL;
    if (prompt != NULL) {
        printf("%s", prompt);
        term_flush();
    }

    u32 i = 0;

    while (1) {
        char c = (char)getchar();
        if (c == '\n' || c == '\r') {
            break;
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

    buf[i] = '\0';
    return buf;
}