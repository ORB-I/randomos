#include <core/std.h>
#include <core/lock.h>
#include <drivers/display/serial.h>
#include <drivers/storage/block.h>
#include <ff16/ff.h>
#include <ff16/diskio.h>

DSTATUS disk_initialize(BYTE pdrv) {
    (void)pdrv;
    return 0;
}

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    return block_read(pdrv, buf, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    return block_write(pdrv, buf, sector, count);
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