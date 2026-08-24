#include <fs.h>
#include <io.h>
#include <mem.h>
#include <sys/sysfn.h>
#include <sys/types.h>
#include <str.h>
#include <sys/process.h>
#include <env.h>
#include <mcrypto.h>

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

u8 _ps_c2nib(char c, int* err) {
    *err = 0;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    *err = 1;
    return 0;
}

int parse_salt(char* str, u8* salt, usize size) {
    if (strlen(str) % 2 != 0) return -1;
    for (usize i = 0; i < size; i++) {
        int err;
        u8 high = _ps_c2nib(str[i], &err);
        if (err) return -1;
        u8 low = _ps_c2nib(str[i+1], &err);
        if (err) return -1;
        salt[i] = ((high & 0x0F) << 4) | (low & 0x0F);
    }
    return 0;
}

int main() {
    void* a2wp = malloc(100000 * 1024);
    if (!a2wp) {
        printf("failed to setup argon2\n");
        return 1;
    }
    crypto_argon2_config a2cfg = {
        .algorithm = CRYPTO_ARGON2_ID,
        .nb_blocks = 100000,
        .nb_passes = 3,
        .nb_lanes = 1
    };
    crypto_argon2_extras a2ex = {
        .key = NULL,
        .ad = NULL,
        .key_size = 0,
        .ad_size = 0
    };

    while (1) {
        char *uname, *pwd;
        if (get_info(&uname, &pwd) < 0) {
            free(a2wp);
            return 1;
        }

        struct passwd pass;
        int ret = getpwnam(uname, &pass);
        switch (ret) {
            case -1:
                free(uname); free(pwd);
                free(a2wp);
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
            printf("encryption not supported yet\n");
            free(uname); free(pwd);
            continue;

            char* salt = strtok(pass.passwd, "$");
            if (strlen(salt) < 32) {
                printf("salt is not at least 16 bytes\n");
                free(uname); free(pwd);
                continue;
            }

            if (strlen(salt) % 2 != 0) {
                printf("invalid salt\n");
                free(uname); free(pwd);
                continue;
            }

            u8* psalt = malloc(strlen(salt)/2);
            if (!psalt) {
                printf("Failed tto allocate salt\n");
                free(uname); free(pwd);
                continue;
            }

            if (parse_salt(salt, psalt, strlen(salt)/2) < 0) {
                printf("Failed to parse salt\n");
                free(uname); free(pwd);
                free(psalt);
                continue;
            }

            char* hash = strtok(NULL, "$");
            if (!hash) {
                printf("Invalid encrypted password\n");
                free(uname); free(pwd);
                free(psalt);
                continue;
            }

            if (strlen(hash) % 2 != 0) {
                printf("invalid hash\n");
                free(uname); free(pwd);
                free(psalt);
                continue;
            }

            u8* phash = malloc(strlen(hash)/2);
            if (!phash) {
                printf("Failed to allocate hash\n");
                free(uname); free(pwd);
                free(psalt);
                continue;
            }

            if (parse_salt(hash, phash, strlen(hash)/2) < 0) {
                printf("Failed to parse hash\n");
                free(uname); free(pwd); free(psalt); free(phash);
                continue;
            }

            crypto_argon2_inputs a2is = {
                .pass = (const u8*)pwd,
                .salt = psalt,
                .pass_size = strlen(pwd),
                .salt_size = strlen(salt) / 2
            };

            u8 nhash[32];
            crypto_argon2(nhash, 32, a2wp, a2cfg, a2is, a2ex);
            if (memcmp(nhash, phash, 32) == 0) {
                free(psalt); free(phash);
                setenv("HOME", pass.home, 1);
                setuid(pass.uid);
                seteuid(pass.uid);
                setgid(pass.gid);
                setegid(pass.gid);
                char* argv[] = {pass.shell, NULL};
                free(uname); free(pwd);
                free(a2wp);
                int ret = execve(pass.shell, argv, environ);
                printf("execve failed (%d)\n", ret);
                return 1;
            } else {
                free(psalt); free(phash);
                free(uname); free(pwd);
                printf("Incorrect password\n");
                continue;
            }
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
                free(uname); free(pwd);
                free(a2wp);
                int ret = execve(pass.shell, argv, environ);
                printf("execve failed (%d)\n", ret);
                return 1;
            }
        }
    }
}