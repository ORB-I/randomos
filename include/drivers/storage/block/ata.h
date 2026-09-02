#pragma once
#include <core/std.h>

void ata_enumerate();
void ata_poll(u8 drv);
int ata_secread(u64 drv, u32 lba, u8* buf);
int ata_secwrite(u64 drv, u32 lba, u8* buf);