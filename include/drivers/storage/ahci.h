#pragma once
#include <core/std.h>

int ahci_init(void);
void ahci_secread(u8 drv, u64 lba, u8* buf);
void ahci_secwrite(u8 drv, u64 lba, u8* buf);
