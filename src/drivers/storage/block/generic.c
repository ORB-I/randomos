#include <core/std.h>
#include <core/printf.h>
#include <drivers/storage/ata.h>
#include <drivers/storage/ahci.h>
#include <drivers/storage/usbmsd.h>

#define BLOCK_OK     0
#define BLOCK_ERR    1
#define BLOCK_WRPRT  2
#define BLOCK_NOTRDY 3
#define BLOCK_INVAL  4

#define DRV_ATA    1
#define DRV_AHCI   2
#define DRV_USBMSD 3

int _drv_type = -1;
int _drv_id = -1;

int block_init() {
    int drv = usbmsd_init();
    if (drv < 0) {
        drv = ahci_init();
        if (drv < 0) {
            drv = ata_init();
            if (drv <= 0) {
                return BLOCK_ERR;
            } else {
                serial_printf("Using ATA%d\n", _drv_id);
                _drv_type = DRV_ATA;
                _drv_id = drv;
                return 0;
            }
        } else {
            serial_printf("Using AHCI\n");
            _drv_type = DRV_AHCI;
            _drv_id = drv;
            return 0;
        }
    } else {
        serial_printf("Using USB MSD\n");
        _drv_type = DRV_USBMSD;
        _drv_id = drv;
         return 0;
    }
}

int block_write(u8 id, const u8* buf, u32 lba, usize cnt) {
    if (_drv_id < 0 || _drv_type < 0) return BLOCK_NOTRDY;

    u8 drv = (id != 0) ? id : (u8)_drv_id;
    void (*fn32)(u8 id, u32 lba, u8* buf) = NULL;
    void (*fn64)(u8 id, u64 lba, u8* buf) = NULL;

    if (_drv_type == DRV_ATA) {
        fn32 = ata_secwrite;
    } else if (_drv_type == DRV_AHCI) {
        fn64 = ahci_secwrite;
    } else if (_drv_type == DRV_USBMSD) {
        fn32 = usbmsd_secwrite;
    } else {
        return BLOCK_INVAL;
    }

    if (fn32) {
        for (usize i = 0; i < cnt; i++) {
            fn32(drv, (u32)(lba + i), (u8*)(buf + (i * 512)));
        }
    } else {
        for (usize i = 0; i < cnt; i++) {
            fn64(drv, (u64)(lba + i), (u8*)(buf + (i * 512)));
        }
    }

    return 0;
}

int block_read(u8 id, u8* buf, u32 lba, usize cnt) {
    u8 drv = (id != 0) ? id : (u8)_drv_id;
    void (*fn32)(u8 id, u32 lba, u8* buf) = NULL;
    void (*fn64)(u8 id, u64 lba, u8* buf) = NULL;
    
    if (_drv_type == DRV_ATA) {
        fn32 = ata_secread;
    } else if (_drv_type == DRV_AHCI) {
        fn64 = ahci_secread;
    } else if (_drv_type == DRV_USBMSD) {
        fn32 = usbmsd_secread;
    } else {
        return BLOCK_INVAL;
    }

    if (fn32) {
        for (usize i = 0; i < cnt; i++) {
            fn32(drv, (u32)(lba + i), (u8*)(buf + (i * 512)));
        }
    } else {
        for (usize i = 0; i < cnt; i++) {
            fn64(drv, (u64)(lba + i), (u8*)(buf + (i * 512)));
        }
    }

    return 0;
}