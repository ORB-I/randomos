#include <time.h>
#include <io.h>
#include <str.h>

int main(int ac, char** av) {
    (void)av;

    ctime_t ct;
    get_ctime(&ct);

    char buf[64];
    usize n = strftime(buf, sizeof(buf), "%A, %B %d, %Y %H:%M:%S %Z", &ct);
    if (n) {
        printf("%s\n", buf);
    } else {
        printf("%s %s %02u %02u:%02u:%02u UTC %u\n",
               "???",
               "???",
               (unsigned)(ct.day + 1),
               (unsigned)ct.hrs,
               (unsigned)ct.min,
               (unsigned)ct.sec,
               (unsigned)ct.yr);
    }

    if (ac > 1 && streq(av[1], "-m")) {
        printf("monotonic: %llu ms\n", (unsigned long long)getclock(CLOCK_MONOMS));
    }

    return 0;
}