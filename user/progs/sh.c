#include <io.h>
#include <stdbool.h>
#include <mem.h>
#include <sys/process.h>
#include <str.h>
#include <fs.h>
#include <env.h>

#define MAX_ARGS 16
s32 parse_args(char* str, char** argv) {
    s32 argc = 0;
    bool inq = false;
    bool inw = false;

    for (char *p = str; *p != '\0'; p++) {
        if (*p == '"') {
            inq = !inq;
            *p = '\0';
            continue;
        }

        if ((*p == ' ' || *p == '\t') && !inq) {
            *p = '\0';
            inw = false;
        } else {
            if (!inw) {
                if (argc < MAX_ARGS) {
                    argv[argc++] = p;
                    inw = true;
                }
            }
        }
    }
    argv[argc] = NULL;
    return argc;
}

typedef struct {
    const char* name;
    int (*fn)(int,char**);
} builtin_t;

int cdfn(int ac, char** av) {
    if (ac < 2) {
        printf("not enough arguments\n");
        return 1;
    }

    if (chdir(av[1]) < 0) {
        printf("cd failed\n");
        return 1;
    }
    return 0;
}

builtin_t builtins[] = {
    {"cd", cdfn}
};

int is_builtin(char* av0) {
    for (int i = 0; i < (int)(sizeof(builtins)/sizeof(builtin_t)); i++) {
        if (streq(builtins[i].name, av0)) {
            return i;
        }
    }
    return -1;
}

char* find_exe(char* av0, int* isalloc) {
    if (strneq("../", av0, 3) || strneq("./", av0, 2) || strneq("/", av0, 1)) {
        struct stat st;
        if (stat(av0, &st) != -1) {
            return av0;
        } else {
            printf("could not find program\n");
            return NULL;
        }
    } else {
        char* _PATH = getenv("PATH");
        if (!_PATH) {
            printf("failed to get PATH\n");
            return NULL;
        }

        char* PATH = strdup(_PATH);
        if (!PATH) {
            printf("failed to get PATH\n");
            return NULL;
        }

        char* tk = strtok(PATH, ":");
        usize av0len = strlen(av0);
        struct stat st;
        while (tk != NULL) {
            char* fpath = malloc(strlen(tk) + av0len + 2);
            if (!fpath) {
                tk = strtok(NULL, ":");
                continue;
            }

            sprintf(fpath, "%s/%s", tk, av0);
            if (stat(fpath, &st) != -1) {
                *isalloc = 1;
                free(PATH);
                return fpath;
            }
            free(fpath);
            tk = strtok(NULL, ":");
        }

        free(PATH);
        return NULL;
    }
}

int main() {
    if (!environ) {
        printf("no environ\n");
    } else {
        char* HOME = NULL;
        if ((HOME = getenv("HOME"))) {
            chdir(HOME);
        }
    }

    while (1) {
        char* argv[MAX_ARGS];

        char* cmd = readline("> ");
        if (!cmd) {
            printf("readline failed\n");
            continue;
        }

        s32 argc = parse_args(cmd, argv);
        if (argc == 0) {
            free(cmd);
            continue;
        }

        int builtin = is_builtin(argv[0]);
        if (builtin < 0) {
            int isalloc = 0;
            char* path = find_exe(argv[0], &isalloc);
            if (path) {
                int pid = newproc(path, argv, environ);
                if (pid < 0) {
                    printf("failed to start program\n");
                    if (isalloc) free(path);
                    continue;
                }

                int ex;
                wait(pid, &ex);
                free(path);
            } else {
                printf("failed to find program\n");
            }
        } else {
            builtins[builtin].fn(argc, argv);
        }
        free(cmd);
    }
}