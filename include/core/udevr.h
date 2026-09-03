#pragma once
#include <core/std.h>

#define UDEV_RD 0x0001
#define UDEV_WR 0x0002
#define UDEV_RW (UDEV_RDO | UDEV_WRO)

#define UDEV_RNG  0x0001
#define UDEV_KLOG 0x0002

typedef u16 udev_t;
typedef ssize (*udevfn_t)(udev_t dev, void* buf, usize sz);

int udevr_init();
int udevr_register(u16 id, const char name[32], u16 flags, udevfn_t read, udevfn_t write);
int udevr_regdev(u16 drv, u16 id);
ssize udevr_read(int fd, void* buf, usize sz);
ssize udevr_write(int fd, void* buf, usize sz);