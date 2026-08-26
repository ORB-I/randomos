#pragma once
#include <core/std.h>

#define BLOCK_OK     0
#define BLOCK_ERR    1
#define BLOCK_WRPRT  2
#define BLOCK_NOTRDY 3
#define BLOCK_INVAL  4

int block_init();
int block_write(u8 id, const u8* buf, u32 lba, usize cnt);
int block_read(u8 id, u8* buf, u32 lba, usize cnt);