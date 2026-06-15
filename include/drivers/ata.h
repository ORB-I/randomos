#pragma once
#include <core/std.h>

int ata_init();
void ata_poll(u8 drv);
void ata_secread(u8 drv, u32 lba, u8* buf);
void ata_secwrite(u8 drv, u32 lba, u8* buf);