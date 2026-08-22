#include <core/std.h>
#include <drivers/storage/ata.h>
#include <drivers/storage/ahci.h>
#include <drivers/storage/usbmsd.h>
#include <ff16/ff.h>
#include <ff16/diskio.h>

typedef DRESULT (*diskio_read_t)(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count);
typedef DRESULT (*diskio_write_t)(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count);

u8 ff16_drive = 0xFF;
static diskio_read_t diskio_rd = 0;
static diskio_write_t diskio_wr = 0;

void ff16_set_drive(u8 drv) { ff16_drive = drv; }

DSTATUS disk_initialize(BYTE pdrv) {
    if (ff16_drive <= 2) {
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    if (ff16_drive <= 2) {
        return 0;
    }
    return STA_NOINIT;
}

static DRESULT ata_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        ata_secread((u8)ff16_drive, (u32)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT ata_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        ata_secwrite((u8)ff16_drive, (u32)(sector + i), (u8*)(buf + (i * 512)));
    }
    return RES_OK;
}

static DRESULT ahci_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        ahci_secread((u8)ff16_drive, (u64)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT ahci_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        ahci_secwrite((u8)ff16_drive, (u64)(sector + i), (u8*)(buf + (i * 512)));
    }
    return RES_OK;
}

static DRESULT usbmsd_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        usbmsd_secread((u8)ff16_drive, (u32)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT usbmsd_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        usbmsd_secwrite((u8)ff16_drive, (u32)(sector + i), (u8*)(buf + (i * 512)));
    }
    return RES_OK;
}

void ff16_set_ahci(void) {
    diskio_rd = ahci_secread_wrap;
    diskio_wr = ahci_secwrite_wrap;
}

void ff16_set_usbmsd(void) {
    diskio_rd = usbmsd_secread_wrap;
    diskio_wr = usbmsd_secwrite_wrap;
}

DRESULT disk_read(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    if (ff16_drive > 2) return RES_NOTRDY;
    diskio_read_t fn = diskio_rd ? diskio_rd : ata_secread_wrap;
    return fn(pdrv, buf, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    if (ff16_drive > 2) return RES_NOTRDY;
    diskio_write_t fn = diskio_wr ? diskio_wr : ata_secwrite_wrap;
    return fn(pdrv, buf, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buf) {
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buf = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buf = 1;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *(LBA_t*)buf = 268435456;
            return RES_OK;
    }
    return RES_PARERR;
}
