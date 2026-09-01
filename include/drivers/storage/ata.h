#pragma once
#include <core/std.h>

int ata_init();
void ata_poll(u8 drv);
int ata_secread(u8 drv, u32 lba, u8* buf);
int ata_secwrite(u8 drv, u32 lba, u8* buf);