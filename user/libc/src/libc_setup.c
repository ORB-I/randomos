#include <sys/types.h>
#include <mem.h>
#include <io.h>
#include <sys/elf.h>
#include <str.h>
#include <sys/sysfn.h>
#include <sys/syscall.h>

extern int main(int argc, char** argv);

void __libc_initenviron();
void __libc_initstdio();
void __libc_initatexit();
void __libc_ldsoinit();

void __libc_finienviron();
void __libc_finistdio();
void __libc_finiatexit();

u64 getauxval(u64 type);
void __ldso_ldcleanup();

u64 __uvmm_map_low__  = 0;
u64 __uvmm_map_high__ = 0;
extern u64 __alloc_anoncurrent;
extern char** __libc_envp__;

void __libc_setupfail() {
    __syscall1(SYS_EXIT, -1);
    __builtin_unreachable();
}

__attribute__((no_stack_protector))
__attribute__((noreturn))
int _libc_setup(int argc, char** argv, char** envp) {
    __uvmm_map_low__ = getauxval(AT_MMAPLOW);
    __uvmm_map_high__ = getauxval(AT_MMAPHIGH);
    __alloc_anoncurrent = __uvmm_map_low__;
    __libc_envp__ = envp;

    __libc_initenviron();
    __libc_initstdio();
    __libc_initatexit();

    int ret = main(argc, argv);

    __libc_finiatexit();
    __libc_finistdio();
    __libc_finienviron();
    __ldso_ldcleanup();

    __syscall1(SYS_EXIT, ret);
    __builtin_unreachable();
}