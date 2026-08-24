#pragma once
#include <sys/types.h>

u64 random64(void);
int random_bytes(u8* buf, usize sz);