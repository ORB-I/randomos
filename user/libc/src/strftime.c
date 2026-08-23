#include <time.h>
#include <str.h>
#include <io.h>

static const char* mon_names[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char* full_mon[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char* wday_names[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char* abbr_wday[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static usize pad2(char* dst, u64 v) {
    dst[0] = (char)('0' + (v / 10) % 10);
    dst[1] = (char)('0' + v % 10);
    return 2;
}
static usize pad4(char* dst, u64 v) {
    char t[4];
    t[0] = (char)('0' + (v / 1000) % 10);
    t[1] = (char)('0' + (v / 100) % 10);
    t[2] = (char)('0' + (v / 10) % 10);
    t[3] = (char)('0' + v % 10);
    memcpy(dst, t, 4);
    return 4;
}

usize strftime(char* dst, usize max, const char* fmt, ctime_t* ct) {
    if (!dst || !fmt || !max) return 0;

    usize out = 0;
    u64 wday = (ct->day + 4) % 7; // 1970-01-01 was Thursday

    while (*fmt && out < max - 1) {
        if (*fmt != '%') {
            dst[out++] = *fmt++;
            continue;
        }
        fmt++;
        if (!*fmt) break;

        switch (*fmt) {
            case 'Y': out += pad4(dst + out, ct->yr); break;
            case 'y': out += pad2(dst + out, ct->yr % 100); break;
            case 'm': out += pad2(dst + out, ct->mon + 1); break;
            case 'd': out += pad2(dst + out, ct->day + 1); break;
            case 'H': out += pad2(dst + out, ct->hrs); break;
            case 'M': out += pad2(dst + out, ct->min); break;
            case 'S': out += pad2(dst + out, ct->sec); break;
            case 'p': {
                if (out + 2 >= max - 1) goto done;
                if (ct->hrs < 12) { dst[out++]='A'; dst[out++]='M'; }
                else { dst[out++]='P'; dst[out++]='M'; }
                break;
            }
            case 'A': {
                const char* s = wday_names[wday];
                while (*s && out < max - 1) dst[out++] = *s++;
                break;
            }
            case 'a': {
                const char* s = abbr_wday[wday];
                while (*s && out < max - 1) dst[out++] = *s++;
                break;
            }
            case 'B': {
                const char* s = full_mon[ct->mon];
                while (*s && out < max - 1) dst[out++] = *s++;
                break;
            }
            case 'b':
            case 'h': {
                const char* s = mon_names[ct->mon];
                while (*s && out < max - 1) dst[out++] = *s++;
                break;
            }
            case 'w': {
                dst[out++] = (char)('0' + wday);
                break;
            }
            case 's': {
                // seconds since epoch
                char tmp[24];
                usize n = 0;
                u64 v = ct->epoch;
                if (v == 0) { tmp[n++] = '0'; }
                else {
                    while (v && n < sizeof(tmp)) {
                        tmp[n++] = (char)('0' + (v % 10));
                        v /= 10;
                    }
                    for (usize i = 0; i < n / 2; i++) {
                        char t = tmp[i]; tmp[i] = tmp[n-1-i]; tmp[n-1-i] = t;
                    }
                }
                for (usize i = 0; i < n && out < max - 1; i++) dst[out++] = tmp[i];
                break;
            }
            case '%': dst[out++] = '%'; break;
            default: dst[out++] = '%'; dst[out++] = *fmt; break;
        }
        fmt++;
    }
done:
    if (out < max) dst[out] = '\0';
    return out;
}