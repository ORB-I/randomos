#include <sys/types.h>
#include <sys/sysfn.h>
#include <stdarg.h>
#include <stddef.h>

void putchar(char c) {
    write(1, &c, 1);
}

static void printf_base_numerical(long n, s32 min_width, char pad) {
    char buf[32];
    s32 i = 0;
    s32 negative = 0;

    if (n == 0) {
        while (min_width-- > 1) {
            putchar(pad);
        }
        putchar('0');
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

    // Padding
    while (i < min_width) {
        buf[i++] = pad;
    }

    while (--i >= 0) {
        putchar(buf[i]);
    }
}

static void printf_base_hex(u64 n, s32 min_width, char pad) {
    const char* hex_digits = "0123456789abcdef";
    char buf[32];
    s32 i = 0;

    if (n == 0) {
        while (min_width-- > 1) {
            putchar(pad);
        }
        putchar('0');
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
        putchar(buf[i]);
    }
}

void vprintf(const char* fmt, va_list lst) {
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
                        printf_base_numerical(va_arg(lst, long), width, pad);
                    } else {
                        printf_base_numerical(va_arg(lst, s32), width, pad);
                    }
                    break;

                case 'u':
                    if (is_long) {
                        printf_base_numerical(va_arg(lst, u64), width, pad);
                    } else {
                        printf_base_numerical(va_arg(lst, u32), width, pad);
                    }
                    break;

                case 'x':
                    if (is_long) {
                        printf_base_hex(va_arg(lst, u64), width, pad);
                    } else {
                        printf_base_hex(va_arg(lst, u32), width, pad);
                    }
                    break;

                case 'p':
                    putchar('0');
                    putchar('x');
                    printf_base_hex((uintptr_t)va_arg(lst, void*), width > 2 ? width - 2 : 0, '0');
                    break;

                case 'c':
                    putchar((char)va_arg(lst, s32));
                    break;

                case 's': {
                    const char* str = va_arg(lst, const char*);
                    if (str == NULL) {
                        str = "(null)";
                    }
                    while (*str) {
                        putchar(*str++);
                    }
                    break;
                }

                case '%':
                    putchar('%');
                    break;

                default:
                    putchar('%');
                    putchar(spec);
                    break;
            }
        } else {
            putchar(*fmt++);
        }
    }
}

void printf(const char* fmt, ...) {
    va_list lst;
    va_start(lst, fmt);

    vprintf(fmt, lst);

    va_end(lst);
}