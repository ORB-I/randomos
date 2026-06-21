#pragma once
#include <sys/types.h>

#define TCTL_FLUSH 0
#define TCTL_CLEAR 1
#define TCTL_SCLR  2
#define TCTL_CCLR  3

#define STDIN  0
#define STDOUT 1
#define STDERR 2

typedef enum {
    TERM_BLACK = 0,
    TERM_RED = 1,
    TERM_GREEN = 2,
    TERM_BROWN = 3,
    TERM_BLUE = 4,
    TERM_MAGENTA = 5,
    TERM_CYAN = 6,
    TERM_GREY = 7,
    TERM_BRIGHT_BLACK = 8,
    TERM_BRIGHT_RED = 9,
    TERM_BRIGHT_GREEN = 10,
    TERM_BRIGHT_BROWN = 11,
    TERM_BRIGHT_BLUE = 12,
    TERM_BRIGHT_MAGENTA = 13,
    TERM_BRIGHT_CYAN = 14,
    TERM_BRIGHT_GREY = 15,
} term_color_t;

ssize read(int fd, void* buf, usize sz);
ssize write(int fd, void* buf, usize sz);
int reboot();
int poweroff();
void sleep(int secs);
int termctl(int code, int arg);