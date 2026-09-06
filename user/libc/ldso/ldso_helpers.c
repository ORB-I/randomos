#include "ldso.h"

HIDDEN ASMFUNC void* ldso_mmap(void* addr, u64 phys, u64 npgs, u64 flags) {
    asm volatile(
        "mov %rcx, %r10\n\t"
        "mov $36, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC void* ldso_munmap(void* addr, u64 npgs, u64 flags) {
    asm volatile(
        "mov $37, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC int ldso_mprotect(void* addr, u64 npgs, u64 flgs) {
    asm volatile(
        "mov $63, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC int ldso_open(const char* path, int flags, u16 mode) {
    asm volatile(
        "mov $4, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC int ldso_close(int fd) {
    asm volatile(
        "mov $5, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC ssize ldso_read(int fd, void* buf, usize sz) {
    asm volatile(
        "mov $2, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC s64 ldso_lseek(int fd, s64 off, int whence) {
    asm volatile(
        "mov $9, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC NORETURN void ldso_exit(int code) {
    asm volatile(
        "mov $1, %rax\n\t"
        "syscall\n\t"
    );
}

HIDDEN usize strlen(const char* str);

HIDDEN void ldso_print(const char* buf) {
    usize len = strlen(buf);
    if (len == 0) return;
    asm volatile(
        "mov $54, %%rax\n\t"
        "syscall\n\t"
        :
        : "D"(buf), "S"(len)
        : "rax", "rcx", "r11", "memory"
    );
}

HIDDEN void ldso_print_hex(u64 val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        buf[2 + (15 - i)] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
    }
    buf[18] = '\0';
    ldso_print(buf);
    ldso_print("\n");
}

HIDDEN void* memset(void* dest, int c, usize n) {
    void* orig = dest;
    u8 val = (u8)c;
    asm volatile(
        "cld\n\t"
        "rep stosb"
        : "+D"(dest), "+c"(n)
        : "a"(val)
        : "memory"
    );
    return orig;
}

HIDDEN void* memcpy(void* dest, const void* src, usize count) {
    void* orig = dest;
    asm volatile(
        "cld\n\t"
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(count)
        :: "memory"
    );
    return orig;
}

HIDDEN ASMFUNC int vmm_setflgs(u64 virt, u64 npgs, u64 flags) {
    asm volatile(
        "mov $63, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN usize strlen(const char* str) {
    if (!str) return 0;
    usize len = 0;
    while (str[len]) len++;
    return len;
}

HIDDEN s32 streq(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (*(const unsigned char*)s1 == *(const unsigned char*)s2);
}