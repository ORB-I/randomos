#pragma once
#include <core/std.h>
#include <drivers/usb/uhci.h>

#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE   0x03
#define SCSI_INQUIRY         0x12
#define SCSI_READ_CAPACITY   0x25
#define SCSI_READ_10         0x28
#define SCSI_WRITE_10        0x2A

#define CSW_CMD_PASSED  0x00
#define CSW_CMD_FAILED  0x01
#define CSW_PHASE_ERROR 0x02

typedef struct {
    u32 signature;
    u32 tag;
    u32 data_len;
    u8  flags;
    u8  lun;
    u8  cdb_len;
    u8  cdb[16];
} __attribute__((packed)) usbmsd_cbw_t;

typedef struct {
    u32 signature;
    u32 tag;
    u32 data_residue;
    u8  status;
} __attribute__((packed)) usbmsd_csw_t;

void usbmsd_enumerate();
int usbmsd_secread(u64 drv, u32 lba, u8* buf);
int usbmsd_secwrite(u64 drv, u32 lba, u8* buf);
