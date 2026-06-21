#include <core/idt.h>
#include <core/panic.h>
#include <core/mem/vmm.h>

#include <drivers/term.h>
#include <drivers/fs.h>
#include <drivers/rtc.h>

#include <lib/sh.h>
#include <lib/loader.h>

#include <lai/helpers/pm.h>

#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_KERNEL_GS_BASE 0xC0000102

extern void syscall_s();
extern __attribute__((aligned(16))) u8 kern_stack[16384];
static u64 gsblk[2];

static inline void wrmsr(u32 msr, u64 val) {
    u32 low = val & 0xFFFFFFFF;
    u32 high = val >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

void init_syscalls() {
    gsblk[0] = 0;
    gsblk[1] = (u64)kern_stack + 16384;

    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
    wrmsr(MSR_LSTAR, (u64)syscall_s);
    wrmsr(MSR_STAR, ((u64)0x1B << 48) | ((u64)0x08 << 32));
    wrmsr(MSR_SFMASK, 0x204);
}

struct sysregs {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, r8, r9, r10, r11;
};

[[noreturn]] void sys_exit(page_table_t* uasp) {
    u64 kesp = (u64)kern_stack + 4096;
    vmm_dasp(uasp);
    asm volatile(
        "mov %0, %%rsp\n\t"
        "push %1\n\t"
        "ret"
        :: "r"(kesp), "r"(sh)
        : "memory"
    );



    panic("System call EXIT failed");
}

void syscall_c(struct sysregs* args) {
    page_table_t* uasp = vmm_cpml4v();
    vmm_skasp();
    switch (args->rax) {
        case 1: sys_exit(uasp);
        case 2: {
            args->rax = read(args->rbx, (u8*)args->rcx, args->rdx);
            return;
        }
        case 3: {
            args->rax = write(args->rbx, (u8*)args->rcx, args->rdx);
            return;
        }
        case 4: {
            args->rax = open((char*)args->rbx, args->rcx);
            return;
        }
        case 5: {
            args->rax = close(args->rbx);
            return;
        }
        case 6: {
            if (open((char*)args->rbx, O_CREAT) < 0) {
                args->rax = -1;
                return;
            } else {
                args->rax = close(args->rax);
                return;
            }
        }
        case 7: {
            args->rax = unlink((char*)args->rbx);
            return;
        }
        case 8: {
            args->rax = chdir((char*)args->rbx);
            return;
        }
        case 9: {
            args->rax = lseek(args->rbx, args->rcx, args->rdx);
            return;
        }
        case 10: {
            args->rax = rename((char*)args->rbx, (char*)args->rcx);
            return;
        }
        case 11: {
            args->rax = mkdir((char*)args->rbx);
            return;
        }
        case 12: {
            args->rax = unlink((char*)args->rbx);
            return;
        }
        case 13: {
            if (lai_acpi_reset() == 0) args->rax = 0;
            else args->rax = -1;
            return;
        }
        case 14: {
            args->rax = stat((char*)args->rbx, (struct stat*)args->rcx);
            return;
        }
        case 15: {
            if (lai_enter_sleep(5) == 0) args->rax = 0;
            else args->rax = -1;
            return;
        }
        case 16: {
            rtc_sleep(args->rbx);
            args->rax = 0;
            return;
        }
        case 17: {
            args->rax = readdir((DIR*)args->rbx, (struct stat*)args->rcx);
            return;
        }
        case 18: {
            args->rax = (u64)opendir((char*)args->rbx);
            return;
        }
        case 19: {
            args->rax = closedir((DIR*)args->rbx);
            return;
        }
        case 20: {
            args->rax = getcwd((char*)args->rbx, args->rcx);
            return;
        }
        case 21: {
            args->rax = sync(args->rbx);
            return;
        }
        case 22: {
            args->rax = trunc(args->rbx);
            return;
        }
        case 23: {
            args->rax = termctl(args->rbx, args->rcx);
        }

        default: args->rax = -1;
    }
}