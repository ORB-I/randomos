#include <core/std.h>

usize strlen(const char* str) {
    const char* ststr = str;
    while (*str != '\0') str++;
    return str - ststr;
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
    u8* d = dest;
    if (((uintptr_t)d & 7) == 0 && (n & 7) == 0) {
        u64 val = ((u64)(u8)c) * 0x0101010101010101ULL;
        u64* d64 = (u64*)d;
        size_t n64 = n / 8;
        for (size_t i = 0; i < n64; i++) d64[i] = val;
        return dest;
    }
    for (size_t i = 0; i < n; i++) d[i] = c;
    return dest;
}

void* memcpy(void* dest, const void* src, usize count) {
    const u8* sp = (const u8*)src;
    u8* dp = (u8*)dest;
    for (usize i = 0; i < count; i++) {
        dp[i] = sp[i];
    }
    return dest;
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