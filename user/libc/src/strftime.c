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
}