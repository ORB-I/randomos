#include <core/std.h>

#include <drivers/ata.h>

#include <ff16/ff.h>
#include <ff16/diskio.h>

u8 ff16_drive;

void ff16_set_drive(u8 drv) { ff16_drive = drv; }

DSTATUS disk_initialize(BYTE _) {
    if (ff16_drive == 1 || ff16_drive == 2) {
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE _) {
    if (ff16_drive == 1 || ff16_drive == 2) {
        return 0;
    }
    return STA_NOINIT;
}

DRESULT disk_read(BYTE _, BYTE* buff, LBA_t sector, UINT count) {
    if (ff16_drive < 0) return RES_NOTRDY;
    for (UINT i = 0; i < count; i++) {
        ata_secread((u8)ff16_drive, sector + i, buff + (i * 512));
    }
    return RES_OK;
}

DRESULT disk_write(BYTE _, const BYTE* buff, LBA_t sector, UINT count) {
    if (ff16_drive < 0) return RES_NOTRDY;
    for (UINT i = 0; i < count; i++) {
        ata_secwrite((u8)ff16_drive, sector + i, (u8*)(buff + (i * 512)));
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE _, BYTE cmd, void* buff) {
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *(LBA_t*)buff = 268435456; 
            return RES_OK;
    }
    return RES_PARERR;
}