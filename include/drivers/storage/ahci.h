#pragma once
#include <core/std.h>

void ahci_enumerate();
int ahci_secread(u64 drv, u64 lba, u8* buf);
int ahci_secwrite(u64 drv, u64 lba, u8* buf);
