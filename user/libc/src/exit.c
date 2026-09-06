#include <sys/types.h>
#include <mem.h>
#include <sys/syscall.h>
#include <exit.h>

usize __libc_num_atexit__ = 0;
void (**__libc_atexits__)(void) = NULL;

void __libc_finienviron();
void __libc_finistdio();
void __libc_finiatexit();

u64 getauxval(u64 type);
void __ldso_ldcleanup();

[[noreturn]] void exit(int c) {
    __libc_finiatexit();
    __libc_finistdio();
    __libc_finienviron();
    __ldso_ldcleanup();

    __syscall1(SYS_EXIT, c);
    __builtin_unreachable();
}

[[noreturn]] void _Exit(int c) {
    __syscall1(SYS_EXIT, c);
    __builtin_unreachable();
}

[[noreturn]] void abort() {
    __syscall1(SYS_EXIT, -1);
    __builtin_unreachable();
}

//void atexit(void(*fn)(void)) {}

void __libc_setupfail();

// eventually, once we write atexit
// these will (de)allocate atexit stuff
void __libc_initatexit() {}
void __libc_finiatexit() {}