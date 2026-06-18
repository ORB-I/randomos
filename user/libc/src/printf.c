#include <sys/types.h>
#include <sys/sysfn.h>
#include <stdarg.h>
#include <stddef.h>

void fputchar(int fd, char c) { write(fd, &c, 1); }

void putchar(char c) { fputchar(STDOUT, c); }

static void printf_base_numerical(int fd, long n, s32 min_width, char pad) {
    char buf[32];
    s32 i = 0;
    s32 negative = 0;

    if (n == 0) {
        while (min_width-- > 1) {
            fputchar(fd, pad);
        }
        fputchar(fd, '0');
        return;
    }

    if (n < 0) {
        negative = 1;
        n = -n;
    }

    while (n != 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    if (negative) {
        buf[i++] = '-';
    }

    while (i < min_width) {
        buf[i++] = pad;
    }

    while (--i >= 0) {
        fputchar(fd, buf[i]);
    }
}

static void printf_base_hex(int fd, u64 n, s32 min_width, char pad) {
    const char* hex_digits = "0123456789abcdef";
    char buf[32];
    s32 i = 0;

    if (n == 0) {
        while (min_width-- > 1) {
            fputchar(fd, pad);
        }
        fputchar(fd, '0');
        return;
    }

    while (n != 0) {
        buf[i++] = hex_digits[n & 0xF];
        n >>= 4;
    }

    while (i < min_width) {
        buf[i++] = pad;
    }

    while (--i >= 0) {
        fputchar(fd, buf[i]);
    }
}

void vfprintf(int fd, const char* fmt, va_list lst) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;

            s32 width = 0;
            char pad = ' ';
            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            s32 is_long = 0;
            if (*fmt == 'l') {
                is_long = 1;
                fmt++;
            }

            char spec = *fmt++;

            switch (spec) {
                case 'd':
                case 'i':
                    if (is_long) {
                        printf_base_numerical(fd, va_arg(lst, long), width, pad);
                    } else {
                        printf_base_numerical(fd, va_arg(lst, s32), width, pad);
                    }
                    break;

                case 'u':
                    if (is_long) {
                        printf_base_numerical(fd, va_arg(lst, u64), width, pad);
                    } else {
                        printf_base_numerical(fd, va_arg(lst, u32), width, pad);
                    }
                    break;

                case 'x':
                    if (is_long) {
                        printf_base_hex(fd, va_arg(lst, u64), width, pad);
                    } else {
                        printf_base_hex(fd, va_arg(lst, u32), width, pad);
                    }
                    break;

                case 'p':
                    fputchar(fd, '0');
                    fputchar(fd, 'x');
                    printf_base_hex(fd, (uintptr_t)va_arg(lst, void*), width > 2 ? width - 2 : 0, '0');
                    break;

                case 'c':
                    fputchar(fd, (char)va_arg(lst, s32));
                    break;

                case 's': {
                    const char* str = va_arg(lst, const char*);
                    if (str == NULL) {
                        str = "(null)";
                    }
                    while (*str) {
                        fputchar(fd, *str);
                        str++;
                    }
                    break;
                }

                case '%':
                    fputchar(fd, '%');
                    break;

                default:
                    fputchar(fd, '%');
                    fputchar(fd, spec);
                    break;
            }
        } else {
            fputchar(fd, *fmt);
            fmt++;
        }
    }
}

void printf(const char* fmt, ...) {
    va_list lst;
    va_start(lst, fmt);
    vfprintf(STDOUT, fmt, lst);
    va_end(lst);
}

void fprintf(int fd, const char* fmt, ...) {
    va_list lst;
    va_start(lst, fmt);
    vfprintf(fd, fmt, lst);
    va_end(lst);
}