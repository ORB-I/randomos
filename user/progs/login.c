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
        close(fd);
        return -2;
    }

    usize size = (usize)st.st_size;
    char* fbuf = malloc(size + 1);
    if (!fbuf) {
        close(fd);
        return -2;
    }

    ssize rd;
    if ((rd = read(fd, fbuf, st.st_size)) < 0 || (usize)rd != size) {
        free(fbuf);
        close(fd);
        return -2;
    }
    fbuf[size] = '\0';
    close(fd);

    // entries are '\n'-terminated; the last one may lack the newline,
    // so i == size also closes a pending entry. entst must advance to
    // the line after each entry or only the first user can ever match.
    // malformed lines are skipped, not fatal.
    char* entst = fbuf;
    for (usize i = 0; i <= size; i++) {
        if (i < size && fbuf[i] != '\n') continue;
        fbuf[i] = '\0';
        if (*entst != '\0' &&
            decode_passwd(entst, buf) == 0 &&
            streq(login, buf->uname)) {
            return 0;   // buf fields point into fbuf; caller must not free it
        }
        entst = &fbuf[i + 1];
    }

    free(fbuf);
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

// decodes exactly `nbytes` from str (no parity check over the rest of
// the string, unlike parse_salt)
int _ps_hx2bin(const char* str, u8* out, usize nbytes) {
    for (usize i = 0; i < nbytes; i++) {
        int err;
        u8 high = _ps_c2nib(str[2*i], &err);
        if (err) return -1;
        u8 low = _ps_c2nib(str[2*i + 1], &err);
        if (err) return -1;
        out[i] = ((high & 0x0F) << 4) | (low & 0x0F);
    }
    return 0;
}

#define ARGON2_HASH_SIZE 32

// verifies `entered` against a stored hash entry (passwd field 2 when
// field 6 is "y"):
//   $argon2id$<blocks>,<passes>,<lanes>$<salt-hex>$<hash-hex>
//
// returns 0 on match, 1 on mismatch, <0 on malformed entry (-1) or
// out of memory (-2)
int verify_passwd(const char* entered, const char* stored) {
    serial_printf("Begin verify password\n");
    static const char prefix[] = "$argon2id$";
    const usize plen = sizeof(prefix) - 1;
    if (strneq(stored, prefix, plen) == 0) return -1;

    // parameters: "<blocks>,<passes>,<lanes>"
    char pbuf[40];
    usize pi = 0;
    usize i = plen;
    while (stored[i] && stored[i] != '$') {
        if (pi + 1 >= sizeof(pbuf)) return -1;
        pbuf[pi++] = stored[i++];
    }
    if (stored[i] != '$') return -1;
    pbuf[pi] = '\0';
    i++; // past '$'

    serial_printf("vfpwd: 1\n");

    char* eptr;
    int blocks = strtoi(pbuf, &eptr);
    if (*eptr != ',' || blocks < 8) return -1;
    int passes = strtoi(eptr + 1, &eptr);
    if (*eptr != ',' || passes < 1) return -1;
    int lanes = strtoi(eptr + 1, &eptr);
    if (*eptr != '\0' || lanes < 1 || lanes > 0xFFFF) return -1;
    if ((u32)blocks < 8u * (u32)lanes || (u64)blocks > (1ULL << 20)) return -1;

    // salt: hex until the next '$'
    usize salt_hexlen = 0;
    while (stored[i + salt_hexlen] && stored[i + salt_hexlen] != '$') salt_hexlen++;
    if (stored[i + salt_hexlen] != '$' || salt_hexlen == 0 || salt_hexlen % 2 != 0) return -1;
    usize salt_size = salt_hexlen / 2;
    if (salt_size < 8 || salt_size > 64) return -1;

    serial_printf("vfpwd: 2\n");

    // hash: exactly ARGON2_HASH_SIZE bytes of hex to end of field
    const char* hash_hex = &stored[i + salt_hexlen + 1];
    usize hash_hexlen = strlen(hash_hex);
    if (hash_hexlen != ARGON2_HASH_SIZE * 2) return -1;

    u8 salt[64];
    u8 want[ARGON2_HASH_SIZE];
    if (_ps_hx2bin(&stored[i], salt, salt_size) < 0) return -1;
    if (_ps_hx2bin(hash_hex, want, ARGON2_HASH_SIZE) < 0) return -1;

    serial_printf("vfpwd: 3\n");

    u64 work_size = (u64)blocks * 1024;
    void* work = malloc(work_size);
    if (!work) return -2;

    crypto_argon2_config cfg = {
        .algorithm = CRYPTO_ARGON2_ID,
        .nb_blocks = (u32)blocks,
        .nb_passes = (u32)passes,
        .nb_lanes  = (u32)lanes,
    };
    crypto_argon2_inputs in = {
        .pass      = (const u8*)entered,
        .pass_size = (u32)strlen(entered),
        .salt      = salt,
        .salt_size = (u32)salt_size,
    };

    serial_printf("vfpwd: 4\n");

    u8 got[ARGON2_HASH_SIZE];
    crypto_argon2(got, ARGON2_HASH_SIZE, work, cfg, in, crypto_argon2_no_extras);
    
    int mismatch = crypto_verify32(got, want);
    crypto_wipe(got, sizeof(got));
    crypto_wipe(want, sizeof(want));
    crypto_wipe(salt, sizeof(salt));
    crypto_wipe(work, work_size);
    free(work);

    return mismatch ? 1 : 0;
}

void ensure_home(char* home) {
    struct stat st;
    if (strlen(home) == 0) return;
    if (stat(home, &st) < 0) {
        char* tmp = strdup(home);
        if (!tmp) return;

        char* p = NULL;
        if (tmp[0] == '/') {
            p = strchr(tmp + 1, '/');
        } else {
            p = strchr(tmp, '/');
        }

        while (p != NULL) {
            *p = '\0';
            if (mkdir(tmp) != 0) {
                if (stat(tmp, &st) < 0) {
                    free(tmp);
                    return;
                }
            }
            *p = '/';
            p = strchr(p+1, '/');
        }

        if (mkdir(tmp) != 0) {
            if (stat(tmp, &st) < 0) {
                free(tmp);
                return;
            }
        }

        free(tmp);
        return;
    }
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

        int ok;
        if (pass.encrypted) {
            int vr = verify_passwd(pwd, pass.passwd);
            if (vr < 0) {
                crypto_wipe(pwd, strlen(pwd));
                free(uname); free(pwd);
                printf(vr == -2 ? "out of memory\n" : "malformed hash entry\n");
                continue;
            }
            ok = (vr == 0);
        } else {
            ok = streq(pass.passwd, pwd);
        }

        if (!ok) {
            crypto_wipe(pwd, strlen(pwd));
            free(uname); free(pwd);
            printf("Incorrect password\n");
            continue;
        }

        crypto_wipe(pwd, strlen(pwd));
        setenv("HOME", pass.home, 1);
        ensure_home(pass.home);
        setuid(pass.uid);
        seteuid(pass.uid);
        setgid(pass.gid);
        setegid(pass.gid);
        char* argv[] = {pass.shell, NULL};
        free(uname); free(pwd);
        printf("starting %s\n", pass.shell);
        int exv = execve(pass.shell, argv, environ);
        printf("execve failed (%d)\n", exv);
        return 1;
    }
}