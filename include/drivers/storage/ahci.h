#pragma once
#include <core/std.h>

int ahci_init(void);
void ahci_secread(u8 drv, u32 lba, u8* buf);
void ahci_secwrite(u8 drv, u32 lba, u8* buf);
