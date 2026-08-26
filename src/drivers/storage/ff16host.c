#include <core/std.h>
#include <core/lock.h>
#include <drivers/display/serial.h>
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
    // fatfs insists on handing us a drive number, but every backend
    // routes through the global ff16_drive instead
    (void)pdrv;
    if (ff16_drive <= 2) {
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;
    if (ff16_drive <= 2) {
        return 0;
    }
    return STA_NOINIT;
}

static DRESULT ata_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        ata_secread(drv, (u32)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT ata_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        ata_secwrite(drv, (u32)(sector + i), (u8*)(buf + (i * 512)));
    }
    return RES_OK;
}

static DRESULT ahci_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        ahci_secread(drv, (u64)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT ahci_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        ahci_secwrite(drv, (u64)(sector + i), (u8*)(buf + (i * 512)));
    }
    return RES_OK;
}

static DRESULT usbmsd_secread_wrap(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        usbmsd_secread(drv, (u32)(sector + i), buf + (i * 512));
    }
    return RES_OK;
}

static DRESULT usbmsd_secwrite_wrap(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    u8 drv = (pdrv != 0) ? pdrv : (u8)ff16_drive;
    for (UINT i = 0; i < count; i++) {
        usbmsd_secwrite(drv, (u32)(sector + i), (u8*)(buf + (i * 512)));
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
    if (ff16_drive > 2 && pdrv == 0) return RES_NOTRDY;
    diskio_read_t fn = diskio_rd ? diskio_rd : ata_secread_wrap;
    return fn(pdrv, buf, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    if (ff16_drive > 2 && pdrv == 0) return RES_NOTRDY;
    diskio_write_t fn = diskio_wr ? diskio_wr : ata_secwrite_wrap;
    return fn(pdrv, buf, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buf) {
    (void)pdrv;
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
            // Standard 128MB virtual disk sector count (262144 sectors) or max supported LBA
            *(LBA_t*)buf = 262144;
            return RES_OK;
    }
    return RES_PARERR;
}

/* backend-agnostic block io so filesystems other than fat can ride the
   same drive selection logic. transfers hold a lock because the ata
   registers are shared state: a preemption between issuing the command
   and draining the data port makes the next reader eat the previous
   sector. spl_lock keeps interrupts off for the same reason */
static spinlock_t blkio_lk;

int storage_blk_read(u64 lba, u32 count, void* buf) {
    if (ff16_drive > 2) return -1;
    diskio_read_t fn = diskio_rd ? diskio_rd : ata_secread_wrap;
    spl_lock(&blkio_lk);
    serial_printf("[dbg] blk_read drv=%d lba=%d cnt=%d\n", ff16_drive, lba, count);
    DRESULT r = fn(ff16_drive, buf, (LBA_t)lba, count);
    serial_printf("[dbg] blk_read done head=%x %x %x %x\n", ((u8*)buf)[0], ((u8*)buf)[1], ((u8*)buf)[2], ((u8*)buf)[3]);
    spl_unlock(&blkio_lk);
    return r == RES_OK ? 0 : -1;
}

int storage_blk_write(u64 lba, u32 count, const void* buf) {
    if (ff16_drive > 2) return -1;
    diskio_write_t fn = diskio_wr ? diskio_wr : ata_secwrite_wrap;
    spl_lock(&blkio_lk);
    DRESULT r = fn(ff16_drive, (const BYTE*)buf, (LBA_t)lba, count);
    spl_unlock(&blkio_lk);
    return r == RES_OK ? 0 : -1;
}
