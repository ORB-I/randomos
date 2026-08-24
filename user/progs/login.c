#include <fs.h>
#include <io.h>
#include <mem.h>
#include <sys/sysfn.h>
#include <sys/types.h>
#include <str.h>
#include <sys/process.h>
#include <env.h>

struct passwd {
    char* uname;
    char* passwd;
    uid_t uid;
    gid_t gid;
    char* fullname;
    int encrypted;
    char* home;
    char* shell;
};

#define DECODE_FIELD(FIELD) strtok(NULL, ":"); if (!FIELD) return -1;
int decode_passwd(char* ent, struct passwd* pwd) {
    usize entlen = strlen(ent);

    char* fields[8] = {NULL};
    usize fidx = 0;

    char* fst = ent;
    for (usize i = 0; i < entlen; i++) {
        if (ent[i] == ':') {
            ent[i] = '\0';
            fields[fidx++] = fst;
            fst = &ent[i + 1];
        }
    }
    fields[fidx++] = fst;

    if (!fields[0] || !fields[1] || 
        !fields[2] || !fields[3] || 
        !fields[4] || !fields[5] || 
        !fields[6] || !fields[7]) {
        return -1;
    }

    char* eptr;

    pwd->uname = fields[0];
    pwd->passwd = fields[1];
    pwd->uid = strtoi(fields[2], &eptr);
    if (*eptr != '\0') return -1;
    pwd->gid = strtoi(fields[3], &eptr);
    if (*eptr != '\0') return -1;
    pwd->fullname = fields[4];
    if (streq(fields[5], "y")) pwd->encrypted = 1;
    else if (streq(fields[5], "n")) pwd->encrypted = 0;
    else return -1;
    pwd->home = fields[6];
    pwd->shell = fields[7];

    return 0;
}

int getpwnam(const char* login, struct passwd* buf) {
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (stat("/etc/passwd", &st) < 0) {
        return -2;
    }

    char* fbuf = malloc(st.st_size + 1);
    if (!fbuf) return -2;

    ssize rd;
    if ((rd = read(fd, fbuf, st.st_size)) < 0 || (usize)rd != st.st_size) {
        free(fbuf);
        return -2;
    }

    char* entst = fbuf;
    for (usize i = 0; i < st.st_size; i++) {
        if (fbuf[i] == '\n') {
            fbuf[i] = '\0';
            if (decode_passwd(entst, buf) < 0) return -1;
            if (streq(login, buf->uname)) {
                return 0;
            }
        }
    }
    return -3;
}

int get_info(char** uname, char** pwd) {
    *uname = readline("user: ");
    if (!*uname) {
        printf("failed to get username\n");
        return -1;
    }

    *pwd = readline("password: ");
    if (!*pwd) {
        printf("failed to get password\n");
        return -1;
    }

    return 0;
}

int main() {
    while (1) {
        char *uname, *pwd;
        if (get_info(&uname, &pwd) < 0) {
            return 1;
        }

        struct passwd pass;
        int ret = getpwnam(uname, &pass);
        switch (ret) {
            case -1:
                free(uname); free(pwd);
                printf("No passwd file or invalid\n");
                return 1;
            case -2:
                free(uname); free(pwd);
                printf("Error\n");
                continue;
            case -3:
                free(uname); free(pwd);
                printf("User does not exist\n");
                continue;
            default: break;
        }

        if (pass.encrypted) {
            printf("encryption is not supported yet\n");
            continue;
        } else {
            if (!streq(pass.passwd, pwd)) {
                free(uname); free(pwd);
                printf("Incorrect password\n");
                continue;
            } else {
                setenv("HOME", pass.home, 1);
                setuid(pass.uid);
                seteuid(pass.uid);
                setgid(pass.gid);
                setegid(pass.gid);
                char* argv[] = {pass.shell, NULL};
                int ret = execve(pass.shell, argv, environ);
                printf("execve failed (%d)\n", ret);
                return 1;
            }
        }
    }
}