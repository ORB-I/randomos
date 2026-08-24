#include <random.h>
#include <sys/types.h>
#include <sys/syscall.h>

u64 random64(void) {
    return __syscall0(SYS_RANDOM64);
}

int random_bytes(u8* buf, usize sz) {
    return __syscall2(SYS_RANDOMBYTES, (u64)buf, (u64)sz);
}