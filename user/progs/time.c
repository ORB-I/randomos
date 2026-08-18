#include <time.h>
#include <io.h>

int main(int ac, char** av) {
    ctime_t ct;
    get_ctime(&ct);
    u64 mono = getclock(CLOCK_MONOMS);
}