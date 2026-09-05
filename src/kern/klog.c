#include <core/udevr.h>
#include <core/cmdline.h>
#include <core/printf.h>
#include <core/kqueue.h>
#include <core/liballoc.h>
#include <core/lock.h>
#include <lib/string.h>

#include <drivers/time/gettimeofday.h>

#define LOGDEV_NONE   0
#define LOGDEV_SERIAL 1
#define LOGDEV_TERM   2
#define LOGDEV_BOTH  (LOGDEV_SERIAL | LOGDEV_TERM)

static int logdev = LOGDEV_BOTH;
static int hasdevent = -1;
static lock_t __kplock = {0};

kqueue_t* devqueue = NULL;
ssize klog_read(udev_t dev, void* buf, usize sz) {
    (void)dev;
    return kqueue_dequeue(devqueue, buf, sz);
}

int kprint_init() {
    const char* dev = cmdline_get("logdev");
    if (dev) {
        if (streq("serial", dev)) {
            logdev = LOGDEV_SERIAL;
        } else if (streq("term", dev)) {
            logdev = LOGDEV_TERM;
        }
    }
    return 0;
}

int kprint_initdev() {
    if (udevr_register(UDEV_KLOG, "klog", UDEV_RD, klog_read, NULL) < 0) {
        hasdevent = -1;
    } else {
        hasdevent = 0;
    }
    
    if (udevr_regdev(UDEV_KLOG, 0) < 0) {
        hasdevent = -1;
    } else {
        hasdevent = 1;
        if (!(devqueue = kqueue_init(1024 * 10))) {
            hasdevent = -1;
        }
    }

    return 0;
}

static int klog_stamp(char* out, usize cap) {
    u64 tod = gettimeofday() % 86400;
    return snprintf(out, cap, "[%02lu:%02lu:%02lu] ",
                    tod / 3600, (tod % 3600) / 60, tod % 60);
}

int kvprint(const char* fmt, va_list ap) {
    int ret = 0;

    char stamp[16];
    int stamp_len = klog_stamp(stamp, sizeof(stamp));
    if (stamp_len < 0) stamp_len = 0;

    va_list sap;
    va_copy(sap, ap);

    u64 rflags = 0;
    lock_acquire(&__kplock, &rflags);
    if (logdev & LOGDEV_SERIAL) {
        va_list sap_serial;
        va_copy(sap_serial, ap);
        serial_printf("%s", stamp);
        ret = serial_vprintf(fmt, sap_serial);
        va_end(sap_serial);
    }
    if (logdev & LOGDEV_TERM) {
        va_list sap_term;
        va_copy(sap_term, ap);
        printf("%s", stamp);
        ret = vprintf(fmt, sap_term);
        va_end(sap_term);
    }

    if (hasdevent == 1) {
        if (!devqueue) {
            lock_release(&__kplock, &rflags);
            return 0;
        }
        
        char* buf = malloc(1024 * 10);

        if (!buf) {
            lock_release(&__kplock, &rflags);
            return 0;
        }

        memcpy(buf, stamp, stamp_len);
        ret = vsnprintf(buf + stamp_len, 1024 * 10 - stamp_len, fmt, sap);
        if (stamp_len + ret > 1024 * 10 - 1) ret = 1024 * 10 - 1 - stamp_len;
        kqueue_enqueue(devqueue, (u8*)buf, stamp_len + ret);
        va_end(sap);
        free(buf);
    }
    lock_release(&__kplock, &rflags);

    return ret;
}

int kprint(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprint(fmt, ap);    
    va_end(ap);
    return ret;
}