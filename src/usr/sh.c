#include <core/std.h>

#include <lib/sh.h>
#include <lib/loader.h>

#include <drivers/kbd.h>
#include <drivers/vga.h>
#include <lib/string.h>
#include <drivers/rtc.h>
#include <drivers/fs.h>

#include <lai/helpers/pm.h>

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

void sh() {
    while (1) {
        char* argv[MAX_ARGS];

        char* cmd = readline("> ");
        if (!cmd) printf("readline failed");
        s32 argc = parse_args(cmd, argv);

        if (argc == 0) {
            printf("no command provided\n");
            //kfree(cmd);
            continue;
        }

        struct stat st;
        if (stat(argv[0], &st) != -1) {
            if (load_program(argv[0], argv) < 0) {
                printf("failed to load program\n");
            }
        } else {
            printf("not found\n");
        }

        //kfree(cmd);
    }
}