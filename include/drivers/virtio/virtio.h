#pragma once
#include <core/std.h>
#include <drivers/pci.h>

#define VIRTIO_VENDOR_ID         0x1AF4

/* VirtIO Device IDs (Legacy / Subsystem) */
#define VIRTIO_DEV_NET           0x1000
#define VIRTIO_DEV_BLOCK         0x1001
#define VIRTIO_DEV_BALLOON       0x1002
#define VIRTIO_DEV_CONSOLE       0x1003
#define VIRTIO_DEV_SCSI          0x1004
#define VIRTIO_DEV_RNG           0x1005
#define VIRTIO_DEV_9P            0x1009

/* VirtIO Modern / Transitional Device IDs (Offset 0x1040) */
#define VIRTIO_DEV_MODERN_NET    0x1041
#define VIRTIO_DEV_MODERN_BLOCK  0x1042
#define VIRTIO_DEV_MODERN_CONSOLE 0x1043
#define VIRTIO_DEV_MODERN_RNG    0x1044
#define VIRTIO_DEV_MODERN_GPU    0x1050

/* VirtIO PCI Legacy Register Offsets (relative to BAR0 I/O base) */
#define VIRTIO_REG_DEVICE_FEATURES 0x00 /* 32-bit R */
#define VIRTIO_REG_GUEST_FEATURES  0x04 /* 32-bit R/W */
#define VIRTIO_REG_QUEUE_ADDRESS   0x08 /* 32-bit R/W (PFN) */
#define VIRTIO_REG_QUEUE_SIZE      0x0C /* 16-bit R */
#define VIRTIO_REG_QUEUE_SELECT    0x0E /* 16-bit R/W */
#define VIRTIO_REG_QUEUE_NOTIFY    0x10 /* 16-bit R/W */
#define VIRTIO_REG_DEVICE_STATUS   0x12 /* 8-bit R/W */
#define VIRTIO_REG_ISR_STATUS      0x13 /* 8-bit R */
#define VIRTIO_REG_CONFIG_LEGACY   0x14 /* Device-specific config space */

/* VirtIO Device Status Bits */
#define VIRTIO_STATUS_RESET        0x00
#define VIRTIO_STATUS_ACKNOWLEDGE  0x01
#define VIRTIO_STATUS_DRIVER       0x02
#define VIRTIO_STATUS_DRIVER_OK    0x04
#define VIRTIO_STATUS_FEATURES_OK  0x08
#define VIRTIO_STATUS_FAILED       0x80

typedef struct {
    u8 bus;
    u8 slot;
    u8 fn;
    u16 devid;
    u16 iobase;
    u8 irq;
} virtio_dev_t;

/* Core VirtIO Device API */
int virtio_find_pci_device(u16 devid, virtio_dev_t* dev);
void virtio_reset(virtio_dev_t* dev);
u8 virtio_get_status(virtio_dev_t* dev);
void virtio_set_status(virtio_dev_t* dev, u8 status);
void virtio_add_status(virtio_dev_t* dev, u8 status);
u32 virtio_get_features(virtio_dev_t* dev);
void virtio_set_features(virtio_dev_t* dev, u32 features);
u8 virtio_read_config8(virtio_dev_t* dev, u8 offset);
u16 virtio_read_config16(virtio_dev_t* dev, u8 offset);
u32 virtio_read_config32(virtio_dev_t* dev, u8 offset);
u64 virtio_read_config64(virtio_dev_t* dev, u8 offset);
u8 virtio_read_isr(virtio_dev_t* dev);
