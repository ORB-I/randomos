#include <core/limreqs.h>
#include <lib/string.h>
#include <core/liballoc.h>

const char* kcmdline = NULL;
void init_cmdline() {
    if (cmdline_req.response && cmdline_req.response->cmdline) {
        kcmdline = (const char*)cmdline_req.response->cmdline;
    }
}

int cmdline_has(const char* name) {
    const char* p = kcmdline;
    usize nlen = strlen(name);

    while (*p) {
        while (*p == ' ') p++;
        if (*p == 0) break;

        const char* st = p;
        while (*p && *p != ' ') p++;

        usize len = p - st;

        if (len == nlen && strneq(st, name, nlen)) return 1;
    }

    return 0;
}

const char* cmdline_get(const char* name) {
    const char* p = kcmdline;
    usize nlen = strlen(name);

    while (*p) {
        while (*p == ' ')
            p++;

        if (*p == '\0')
            break;

        const char* start = p;

        while (*p && *p != ' ')
            p++;

        usize len = p - start;

        if (len > nlen && !strncmp(start, name, nlen) && start[nlen] == '=') {
            const char* valst = start + nlen + 1;
            while (*p && *p != ' ') p++;
            usize vallen = p - valst;
            char* str = malloc(vallen + 1);
            if (!str) return NULL;
            memcpy(str, valst, vallen);
            str[vallen] = '\0';
            return str;
        }
    }

    return NULL;
}