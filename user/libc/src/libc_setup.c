#include <sys/types.h>
#include <mem.h>
#include <io.h>
#include <sys/elf.h>
#include <str.h>

extern int main(int argc, char** argv);
char** environ = NULL;
usize __libc_environ_size__ = 0;
// int errno = 0; we'll do this once kernel returns proper error codes

u64 __uvmm_map_low__  = 0;
u64 __uvmm_map_high__ = 0;
extern u64 __alloc_anoncurrent;

char** __libc_getenviron() {
    return environ;
}

extern u64 __ldso_getauxval(u64 type);
u64 getauxval(u64 type) {
    return __ldso_getauxval(type);
}

int _libc_setup(int argc, char** argv, char** envp) {
    __uvmm_map_low__ = getauxval(AT_MMAPLOW);
    __uvmm_map_high__ = getauxval(AT_MMAPHIGH);
    __alloc_anoncurrent = __uvmm_map_low__;

    if (!envp) {
        environ = NULL;
        goto runmain;
    }

    usize nenvp = 0;
    while (envp[nenvp] != NULL) {
        nenvp++;
    }
    __libc_environ_size__ = nenvp;

    environ = malloc((nenvp + 1) * sizeof(char*));
    if (!environ) {
        printf("libc abort: no environment\n");
        return 127;
    }

    for (usize i = 0; i < nenvp; i++) {
        environ[i] = strdup(envp[i]);
        if (!environ[i]) {
            printf("libc abort: no environment\n");
            return 127;
        }
    }

    environ[nenvp] = NULL;
runmain: {
    int ret = main(argc, argv);
    if (environ) {
        for (usize i = 0; i < nenvp; i++) {
            free(environ[i]);
        }
        free(environ);
    }
    return ret;
}
}