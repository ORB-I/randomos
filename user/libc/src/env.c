#include <sys/types.h>
#include <str.h>
#include <mem.h>
#include <io.h>

extern char** environ;
extern usize __libc_environ_size__;

static int _env_nameok(char* name) {
    return (name != NULL && name[0] != '\0' && strchr(name, '=') == NULL);
}

char* getenv(char* name) {
    if (!_env_nameok(name)) return NULL;
    usize namelen = strlen(name);
    for (usize i = 0; environ[i] != NULL; i++) {
        if (strneq(environ[i], name, namelen) && environ[i][namelen] == '=') {
            return environ[i] + namelen + 1;
        }
    }
    return NULL;
}

int setenv(char* name, char* val, int overwrite) {
    if (!_env_nameok(name)) return -1;
    usize namelen = strlen(name);
    usize vallen = strlen(val);

    ssize fidx = -1;

    for (usize i = 0; i < __libc_environ_size__; i++) {
        if (strneq(environ[i], name, namelen) && environ[i][namelen] == '=') {
            fidx = (ssize)i;
        }
    }

    if (fidx != -1) {
        if (!overwrite) return -1;
        if (strlen(environ[fidx] + namelen + 1) == vallen) {
            memcpy(environ[fidx] + namelen + 1, val, vallen);
        } else {
            char* nent = malloc(namelen + vallen + 2);
            if (!nent) return -1;
            sprintf(nent, "%s=%s", name, val);
            environ[fidx] = nent;
            return 0;
        }
    }

    char* nent = malloc(namelen + vallen + 2);
    if (!nent) return -1;
    sprintf(nent, "%s=%s", name, val);

    char** nenviron = realloc(environ, (__libc_environ_size__ + 2) * sizeof(char*));
    if (!nenviron) {
        free(nent);
        return -1;
    }

    environ = nenviron;
    environ[__libc_environ_size__] = nent;
    environ[++__libc_environ_size__] = NULL;

    return 0;
}