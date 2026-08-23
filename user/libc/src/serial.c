#include <io.h>
#include <sys/syscall.h>

void serial_write(void* buf, usize sz) {
    __syscall2(SYS_SERIALWRITE, (u64)buf, sz);
}