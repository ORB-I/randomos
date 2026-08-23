#include <sys/types.h>
#include <mem.h>
#include <io.h>
#include <str.h>

extern int main(int argc, char** argv);
char** environ = NULL;
usize __libc_environ_size__ = 0;
// int errno = 0; we'll do this once kernel returns proper error codes

char** __libc_getenviron() {
    return environ;
}

int _libc_setup(int argc, char** argv, char** envp) {
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