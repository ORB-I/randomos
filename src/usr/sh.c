#include <core/mem.h>
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

s32 sleep(s32 ac, char** av) {
    if (ac < 2) {
        printf("not enough args\n");
        return 1;
    }

    s32 nsecs = atoi(av[1]);
    if (nsecs < 0) {
        printf("Cannot sleep with negative seconds");
        return 1;
    }

    rtc_sleep(nsecs);
    return 0;
}

s32 reboot(s32 ac, char** av) {
    (void)ac;(void)av;
    lai_acpi_reset();
    return 1;
}

s32 poweroff(s32 ac, char** av) {
    (void)ac;(void)av;
    printf("shutdown\n");
    if (lai_enter_sleep(5) != 0) {
        printf("Poweroff failed\n");
    }
    return 1;
}

s32 echo(s32 ac, char** av) {
    for (s32 i = 1; i < ac; i++) {
        printf("%s", av[i]);
    }
    vga_putchar('\n');
    return 0;
}

s32 ls(s32 ac, char** av) {
    if (ac < 2) {
        printf("not enough args\n");
        return 1;
    }

    DIR* dir = opendir(av[1]);
    if (!dir) {
        printf("Failed to open dir\n");
        return 1;
    }

    struct stat st;
    while (readdir(dir, &st) != -1) {
        printf("%s\n", st.st_name);
    }

    return 0;
}

s32 touch(s32 ac, char** av) {
    if (ac < 2) {
        printf("not enough args\n");
        return 1;
    }

    int fd = open(av[1], O_CREAT);
    if (fd < 0) {
        printf("failed to create\n");
        return 1;
    }

    close(fd);
    return 0;
}

typedef struct {
    char* name;
    s32 (*cmd)(s32,char**);
    bool needdrv;
} builtin_t;

builtin_t builtins[] = {
    {"sleep", sleep, false},
    {"reboot", reboot, false},
    {"poweroff", poweroff, false},
    {"echo", echo, false},
    {"ls", ls, true},
    {"touch", touch, true}
};

const usize nbuiltins = sizeof(builtins) / sizeof(builtin_t);

bool hasdrv_g = false;
bool hasdrv_g_set = false;

void sh(bool hasdrv) {
    if (!hasdrv_g_set) {
        hasdrv_g = hasdrv;
        hasdrv_g_set = true;
    }
    
    while (1) {
        char* argv[MAX_ARGS];

        char* cmd = readline("> ");
        if (!cmd) printf("readline failed");
        s32 argc = parse_args(cmd, argv);

        if (argc == 0) {
            printf("no command provided\n");
            kfree(cmd);
            continue;
        }

        bool found = false;
        for (usize i = 0; i < nbuiltins; i++) {
            if (streq(builtins[i].name, argv[0])) {
                found = true;
                if (builtins[i].needdrv && !hasdrv_g) {
                    printf("this command requires a drive\n");
                } else {
                    builtins[i].cmd(argc, argv);
                }
            }
        }

        if (!found) {
            struct stat st;
            if (stat(argv[0], &st) != -1) {
                if (load_program(argv[0], argv) < 0) {
                    printf("failed to load program\n");
                }
            } else {
                printf("not found\n");
            }
        }

        kfree(cmd);
    }
}