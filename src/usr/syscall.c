#include <core/idt.h>
#include <core/panic.h>
#include <core/mem/vmm.h>

#include <drivers/term.h>
#include <drivers/fs.h>
#include <drivers/rtc.h>

#include <lib/sh.h>
#include <lib/loader.h>
#include <core/asmh.h>

#include <lai/helpers/pm.h>

#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_KERNEL_GS_BASE 0xC0000102

extern void syscall_s();
extern __attribute__((aligned(16))) u8 kern_stack[16384];
static u64 gsblk[2];

void init_syscalls() {
    gsblk[0] = 0;
    gsblk[1] = (u64)kern_stack + 16384;

    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
    wrmsr(MSR_LSTAR, (u64)syscall_s);
    wrmsr(MSR_STAR, ((u64)0x1B << 48) | ((u64)0x08 << 32));
    wrmsr(MSR_SFMASK, 0x204);
}

struct sysregs {
    u64 num, a0, a1, a2, a3, a4, a5;
    u64 __a1, __r11;
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
    switch (args->num) {
        case 1: sys_exit(uasp);
        case 2: {
            args->num = read(args->a0, (u8*)args->a1, args->a2);
            return;
        }
        case 3: {
            args->num = write(args->a0, (u8*)args->a1, args->a2);
            return;
        }
        case 4: {
            args->num = open((char*)args->a0, args->a1);
            return;
        }
        case 5: {
            args->num = close(args->a0);
            return;
        }
        case 6: {
            if (open((char*)args->a0, O_CREAT) < 0) {
                args->num = -1;
                return;
            } else {
                args->num = close(args->num);
                return;
            }
        }
        case 7: {
            args->num = unlink((char*)args->a0);
            return;
        }
        case 8: {
            args->num = chdir((char*)args->a0);
            return;
        }
        case 9: {
            args->num = lseek(args->a0, args->a1, args->a2);
            return;
        }
        case 10: {
            args->num = rename((char*)args->a0, (char*)args->a1);
            return;
        }
        case 11: {
            args->num = mkdir((char*)args->a0);
            return;
        }
        case 12: {
            args->num = unlink((char*)args->a0);
            return;
        }
        case 13: {
            if (lai_acpi_reset() == 0) args->num = 0;
            else args->num = -1;
            return;
        }
        case 14: {
            args->num = stat((char*)args->a0, (struct stat*)args->a1);
            return;
        }
        case 15: {
            if (lai_enter_sleep(5) == 0) args->num = 0;
            else args->num = -1;
            return;
        }
        case 16: {
            rtc_sleep(args->a0);
            args->num = 0;
            return;
        }
        case 17: {
            args->num = readdir((DIR*)args->a0, (struct stat*)args->a1);
            return;
        }
        case 18: {
            args->num = (u64)opendir((char*)args->a0);
            return;
        }
        case 19: {
            args->num = closedir((DIR*)args->a0);
            return;
        }
        case 20: {
            args->num = getcwd((char*)args->a0, args->a1);
            return;
        }
        case 21: {
            args->num = sync(args->a0);
            return;
        }
        case 22: {
            args->num = trunc(args->a0);
            return;
        }
        case 23: {
            args->num = termctl(args->a0, args->a1);
        }

        default: args->num = -1;
    }
}