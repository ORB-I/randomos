#pragma once
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
[[noreturn]] void __assert_fail(const char* expr, const char* file, int line, const char* func);
#define assert(expr) \
    ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif