#include <limits.h>
#include <ctype.h>

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long result = 0;
    int neg = 0;

    while (isspace((unsigned char)*s))  s++;

    if (*s == '+' || *s == '-') {
        if (*s == '-') neg = 1;
        s++;
    }

    if (base == 0) {
        if (*s == '0') {
            if (s[1] == 'x' || s[1] == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    }

    const char *start = s;

    while (*s) {
        int digit;

        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;

        if (digit >= base) break;

        if (result > (ULONG_MAX - (unsigned long)digit) / (unsigned long)base) { 
            result = ULONG_MAX;

            while (*s) {
                if (*s >= '0' && *s <= '9') digit = *s - '0';
                else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
                else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
                else break;

                if (digit >= base) break;

                s++;
            }

            break;
        }

        result = result * (unsigned long)base + (unsigned long)digit;
        s++;
    }

    if (endptr) *endptr = (char *)(start == s ? nptr : s);
    if (neg) result = 0UL - result;

    return result;
}

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long result = 0;
    int neg = 0;

    while (isspace((unsigned char)*s)) s++;

    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
    }

    if (base == 0) {
        if (*s == '0') {
            if (s[1] == 'x' || s[1] == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    }

    const char* digits = s;

    while (*s) {
        int digit;

        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;

        if (digit >= base) break;

        result = result * (unsigned long)base + (unsigned long)digit;
        s++;
    }

    if (endptr) *endptr = (char *)(digits == s ? nptr : s);
    if (neg) return -(long)result;
    return (long)result;
}