#include <time.h>
#include <mem.h>
#include <io.h>
#include <str.h>

const char* _strftime_monnames[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

const char* get_timestr(ctime_t* ct) {
    usize len = strlen(_strftime_monnames[ct->mon]) + 18;
    char* str = malloc(len + 1);

    snprintf(str, len + 1, "%s %02d, %4d %02d:%02d:%02d",
        _strftime_monnames[ct->mon], ct->day + 1, ct->yr,
        ct->hrs, ct->min, ct->sec);
    
    return str;
}

usize strftime(char* str, usize size, const char* fmt, ctime_t* ct) {
    return 0;
}