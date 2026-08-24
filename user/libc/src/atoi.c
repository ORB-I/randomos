#include <sys/types.h>

int strtoi(char* ptr, char** eptr) {
    int sign = 1;
    int val = 0;

     while (*ptr == ' ' || *ptr == '\t' ||
           *ptr == '\n' || *ptr == '\r') {
        ptr++;
    }

    if (*ptr == '-') {
        sign = -1;
        ptr++;
    } else if (*ptr == '+') {
        ptr++;
    }

    while (*ptr >= '0' && *ptr <= '9') {
        val = val * 10 + (*ptr - '0');
        ptr++;
    }

    if (eptr) *eptr = ptr;

    return val * sign;
}