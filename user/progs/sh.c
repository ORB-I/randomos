#include <io.h>
#include <stdbool.h>
#include <mem.h>
#include <sys/sysfn.h>
#include <fs.h>

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

int main() {
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

        struct stat st;
        if (stat(argv[0], &st) != -1) {
            int pid = newproc(argv[0], argv);
            if (pid < 0) {
                printf("failed to start program\n");
            }
            wait(pid);
        } else {
            printf("no such file\n");
        }

        free(cmd);
    }
}