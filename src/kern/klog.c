#include <core/udevr.h>
#include <core/cmdline.h>
#include <core/printf.h>
#include <core/kqueue.h>
#include <core/liballoc.h>
#include <lib/string.h>

#include <drivers/time/gettimeofday.h>

static int hasdevent = -1;

kqueue_t* devqueue = NULL;
ssize klog_read(udev_t dev, void* buf, usize sz) {
    (void)dev;
    return kqueue_dequeue(devqueue, buf, sz);
}

int kprint_init() {
    // Kernel log is unconditionally mirrored to both the serial port and the
    // framebuffer terminal (see kvprint), so boot progress and panics are
    // visible on-screen even on bare metal. `logdev` is no longer consulted.
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

// The wall clock is anchored on the RTC, which only ticks once per second,
// so log timestamps are hour/minute/second and never show subsecond digits.
static int klog_stamp(char* out, usize cap) {
    u64 tod = gettimeofday() % 86400;
    return snprintf(out, cap, "[%02llu:%02llu:%02llu] ",
                    tod / 3600, (tod % 3600) / 60, tod % 60);
}

int kvprint(const char* fmt, va_list ap) {
    int ret = 0;

    char stamp[16];
    int stamp_len = klog_stamp(stamp, sizeof(stamp));
    if (stamp_len < 0) stamp_len = 0;

    // Mirror every kernel log line to both the serial port and the framebuffer terminal
    {
        va_list ap_serial;
        va_copy(ap_serial, ap);
        serial_printf("%s", stamp);
        ret = serial_vprintf(fmt, ap_serial);
        va_end(ap_serial);
    }
    {
        va_list ap_term;
        va_copy(ap_term, ap);
        printf("%s", stamp);
        ret = vprintf(fmt, ap_term);
        va_end(ap_term);
    }

    if (hasdevent == 1) {
        if (!devqueue) return 0;
        char* buf = malloc(1024 * 10);
        if (!buf) return 0;

        memcpy(buf, stamp, stamp_len);
        ret = vsnprintf(buf + stamp_len, 1024 * 10 - stamp_len, fmt, ap);
        if (stamp_len + ret > 1024 * 10 - 1) ret = 1024 * 10 - 1 - stamp_len;

        kqueue_enqueue(devqueue, (u8*)buf, stamp_len + ret);
        free(buf);
    }

    return ret;
}

int kprint(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprint(fmt, ap);    
    va_end(ap);
    return ret;
}