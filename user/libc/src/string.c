#include <str.h>
#include <stddef.h>
#include <mem.h>

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

void* memset(void* dest, int c, usize n) {
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

char* strchr(const char* str, char c) {
    while (*str != '\0') {
        if (*str == c) return (char*)str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return NULL;
}

char* strdup(char* str) {
    usize len = strlen(str);
    char* mem = malloc(len + 1);
    if (!mem) return NULL;
    memcpy(mem, str, len + 1);
    return mem;
}

static int _strtok_isdelim(char c, const char* delim) {
    while (*delim != '\0') {
        if (c == *delim) {
            return 1;
        }
        delim++;
    }
    return 0;
}

char* strtok(char* str, const char* delim) {
    static char* ntok = NULL;
    if (str) {
        ntok = str; 
    }

    if (!ntok || *ntok == '\0') {
        return NULL;
    }

    while (*ntok && _strtok_isdelim(*ntok, delim)) {
        ntok++;
    }

    if (*ntok == '\0') {
        return NULL;
    }

    char* tkstart = ntok;
    while (*ntok) {
        if (_strtok_isdelim(*ntok, delim)) {
            *ntok = '\0';
            ntok++;
            return tkstart;
        }
        ntok++;
    }
    return tkstart;
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

void* memchr(const void* ptr, int c, usize n) {
    const unsigned char* p = ptr;
    unsigned char ch = (unsigned char)c;

    for (usize i = 0; i < n; i++) {
        if (p[i] == ch) return (void*)(p + i);
    }
    return NULL;
}

usize strcspn(const char* s, const char* rj) {
    usize i;

    for (i = 0; s[i] != '\0'; i++) {
        for (const char* p = rj; *p != '\0'; p++) {
            if (s[i] == *p) return i;
        }
    }

    return i;
}

char* strcat(char* dest, const char* src) {
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
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

char* strncpy(char* dst, const char* src, usize sz) {
    usize sl = strlen(src);
    int term = 1;
    if (sl > sz) {
        sl = sz;
        term = 0;
    }

    for (usize i = 0; i < sl; i++) {
        dst[i] = src[i];
    }

    if (term) {
        dst[sl] = '\0';
    }

    if (sl < sz) {
        memset(dst + sl, 0, sz-sl);
    }

    return dst;
}

char* strcpy(char* dst, const char* src) {
    usize sl = strlen(src);
    for (usize i = 0; i < sl; i++) {
        dst[i] = src[i];
    }
    return dst;
}

char* strpbrk(const char* s, const char* cs) {
    for (; *s; s++) {
        for (const char *p = cs; *p; p++) {
            if (*s == *p) return (char *)s;
        }
    }
    return NULL;
}

usize strspn(const char* s, const char* cs) {
    usize i;
    for (i = 0; s[i] != '\0'; i++) {
        const char *p;
        for (p = cs; *p != '\0'; p++) {
            if (s[i] == *p) break;
        }
        if (*p == '\0') break;
    }
    return i;
}

usize strlcpy(char* dst, const char* src, usize size) {
    usize len = 0;

    while (src[len]) len++;
    if (size) {
        usize n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }

    return len;
}

usize strnlen(const char *s, usize maxlen) {
    usize i;
    for (i = 0; i < maxlen && s[i]; i++);
    return i;
}