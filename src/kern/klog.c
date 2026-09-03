#include <core/udevr.h>
#include <core/cmdline.h>
#include <core/printf.h>
#include <core/kqueue.h>
#include <core/liballoc.h>
#include <lib/string.h>

#define LOGDEV_NONE   0
#define LOGDEV_SERIAL 1
#define LOGDEV_TERM   2

static int logdev = LOGDEV_NONE;
static int hasdevent = -1;

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

int kvprint(const char* fmt, va_list ap) {
    va_list ap1;
    va_copy(ap1, ap);

    int ret = 0;
    if (logdev == LOGDEV_SERIAL) {
        ret = serial_vprintf(fmt, ap);
    } else if (logdev == LOGDEV_TERM) {
        ret = vprintf(fmt, ap);
    }

    if (hasdevent == 1) {
        if (!devqueue) return 0;
        char* buf = malloc(1024 * 10);
        if (!buf) return 0;

        ret = vsnprintf(buf, 1024 * 10, fmt, ap1);

        kqueue_enqueue(devqueue, (u8*)buf, ret);
        free(buf);
    }

    va_end(ap1);
    return ret;
}

int kprint(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprint(fmt, ap);    
    va_end(ap);
    return ret;
}