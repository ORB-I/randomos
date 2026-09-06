#include <assert.h>
#include <io.h>
#include <exit.h>

[[noreturn]] void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    printf("assertation failed: %s:%d: %s: %s\n",
        file, line, func, expr);
    abort();
}