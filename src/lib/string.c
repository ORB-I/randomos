#include <core/std.h>
#include <core/liballoc.h>

usize strlen(const char* str) {
    const char* orig = str;
    asm volatile(
        "cld\n\t"
        "repne scasb\n\t"
        : "+D"(str)
        : "a"(0), "c"(-1)
        : "memory", "cc"
    );
    return (str - orig) - 1;
}

s32 streq(const char* s1, const char* s2) {
    usize s1sz = strlen(s1);
    usize s2sz = strlen(s2);

    if (s1sz != s2sz) return 0;

    for (usize i = 0; i < s1sz; i++) {
        if (s1[i] != s2[i]) return 0;
    }

    return 1;
}

s32 strneq(const char* s1, const char* s2, usize n) {
    for (usize i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

s32 atoi(const char* str) {
    s32 res = 0;
    s32 sign = 1;

    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        res = (res * 10) + (*str - '0');
        str++;
    }

    return sign * res;
}

void* memset(void* dest, int c, size_t n) {
    void* orig = dest;
    u8 val = (u8)c;
    asm volatile(
        "cld\n\t"
        "rep stosb"
        : "+D"(dest), "+c"(n)
        : "a"(val)
        : "memory"
    );
    return orig;
}

void* memcpy(void* dest, const void* src, usize count) {
    void* orig = dest;
    asm volatile(
        "cld\n\t"
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(count)
        :: "memory"
    );
    return orig;
}

int memcmp(const void* s1, const void* s2, usize n) {
    const u8* p1 = (const u8*)s1;
    const u8* p2 = (const u8*)s2;
    for (usize i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void* memmove(void* dst, const void* src, usize n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;

    if (d == s || n == 0) {
        return dst;
    }

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n - 1;
        s += n - 1;
        while (n--) {
            *d-- = *s--;
        }
    }

    return dst;
}

char* strchr(const char* str, char c) {
    while (*str != '\0') {
        if (*str == c) return (char*)str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return NULL;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }

    return *s1 - *s2;
}

int strncmp(const char* s1, const char* s2, usize n) {
    while (n > 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++; s2++; n--;
    }

    return 0;
}

char* strdup(const char* str) {
    usize sz = strlen(str) + 1;
    char* nstr = malloc(sz);
    if (!nstr) return NULL;
    memcpy(nstr, str, sz);
    return nstr;
}

char* strcpy(char* dst, const char* str) {
    usize sz = strlen(str) + 1;
    memcpy(dst, str, sz);
    return dst;
}