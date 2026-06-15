#include <core/mem.h>

#include <drivers/vga.h>
#include <drivers/kbd.h>

#define INITBUFSZ 256

char* readline(const char* prompt) {
    u32 bufsz = INITBUFSZ;
    char* buf = (char*)kmalloc(bufsz);

    if (!buf) return NULL;
    if (prompt != NULL) {
        printf("%s", prompt);
        vga_flush();
    }

    u32 i = 0;

    while (1) {
        char c = (char)getchar();
        if (c == '\n' || c == '\r') {
            break;
        }

        if (i >= bufsz - 1) {
            char* newptr = (char*)krealloc(buf, bufsz + 256);
            if (newptr == NULL) {
                kfree(buf);
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