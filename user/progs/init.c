#include <io.h>
#include <sys/process.h>
#include <sys/sysfn.h>
#include <str.h>
#include <mem.h>

const char* banner_lines[] = {
    " /$$$$$$$                            /$$                          /$$$$$$   /$$$$$$",
    "| $$__  $$                          | $$                         /$$__  $$ /$$__  $$",
    "| $$  \\ $$  /$$$$$$  /$$$$$$$   /$$$$$$$  /$$$$$$  /$$$$$$/$$$$ | $$  \\ $$| $$  \\__/",
    "| $$$$$$$/ |____  $$| $$__  $$ /$$__  $$ /$$__  $$| $$_  $$_  $$| $$  | $$|  $$$$$$",
    "| $$__  $$  /$$$$$$$| $$  \\ $$| $$  | $$| $$  \\ $$| $$ \\ $$ \\ $$| $$  | $$ \\____  $$",
    "| $$  \\ $$ /$$__  $$| $$  | $$| $$  | $$| $$  | $$| $$ | $$ | $$| $$  | $$ /$$  \\ $$",
    "| $$  | $$|  $$$$$$$| $$  | $$|  $$$$$$$|  $$$$$$/| $$ | $$ | $$|  $$$$$$/|  $$$$$$/",
    "|__/  |__/ \\_______/|__/  |__/ \\_______/ \\______/ |__/ |__/ |__/ \\______/  \\______/"
};

const usize max_bw = 85;
const usize nlines = 8;

void print_banner() {
    term_pos_t sz;
    termctl(TCTL_GETSZ, (u64)&sz);
    serial_printf("terminal size: x=%lu  y=%lu\n", (unsigned long)sz.x, (unsigned long)sz.y);

    usize bufsz = (sz.x + 1) * 2;
    for (usize i = 0; i < nlines; i++) {
        usize len = strlen(banner_lines[i]);
        usize pad = (sz.x > len) ? (sz.x - len) / 2 : 0;
        bufsz += pad + len + 1;
    }
    bufsz += 1; 

    char* buf = malloc(bufsz);
    if (!buf) { return; }

    usize bufi = 0;

    serial_printf("buffer size %lu at %p\n", bufsz, buf);

    serial_printf("top banner\n");
    memset(buf + bufi, '=', sz.x);
    bufi += sz.x;
    buf[bufi++] = '\n';

    for (usize i = 0; i < nlines; i++) {
        serial_printf("line %d\n", i);
        usize len = strlen(banner_lines[i]);
        usize pad = (sz.x > len) ? (sz.x - len) / 2 : 0;

        memset(buf + bufi, ' ', pad);
        bufi += pad;
        memcpy(buf + bufi, banner_lines[i], len);
        bufi += len;
        buf[bufi++] = '\n';
    }

    serial_printf("bottom banner\n");
    memset(buf + bufi, '=', sz.x);
    bufi += sz.x;
    buf[bufi++] = '\n';

    buf[bufi] = '\0';

    write(STDOUT, buf, bufi);
    termctl(TCTL_FLUSH, 0);
    free(buf);
}

int main() {
    char* shargv[] = {"/bin/login", NULL};
    char* shenvp[] = {"PATH=/bin:/sbin", NULL};
    int pid = 0;

    print_banner();

    while (1) {
        if ((pid = newproc("/bin/login", shargv, shenvp)) < 0) {
            printf("failed to start login\n");
            for (;;);
        }
        int exit;
        wait(pid, &exit);
        printf("login exited with code %d\n", exit);
    }
    for (;;);
}