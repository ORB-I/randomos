#include "core/std.h"
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

#define MSR_EFER          0xC0000080
#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_KERNEL_GS_BASE 0xC0000102
#define MSR_USER_GS_BASE 0xC0000101
#define MSR_IA32_FMASK 0xC0000084

extern void syscall_s();
extern __attribute__((aligned(16))) u8 kern_stack[16384];
static u64 gsblk[2];

void init_syscalls() {
    gsblk[0] = 0x00007FFFFFFFF000;
    gsblk[1] = (u64)kern_stack + 16384;

    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
    wrmsr(MSR_LSTAR, (u64)syscall_s);
    wrmsr(MSR_STAR, ((u64)0x1B << 48) | ((u64)0x08 << 32));
    wrmsr(MSR_SFMASK, 0x204);
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);
    wrmsr(MSR_IA32_FMASK, 0x200); 
}

struct sysregs {
    u64 num, a0, a1, a2, a3, a4, a5;
    u64 __es, __ds, __rflags, __rip;
};

[[noreturn]] void sys_exit(page_table_t* uasp) {
    vmm_dasp(uasp);

    u64 krsp = (u64)(kern_stack + sizeof(kern_stack));
    reset_rsp(krsp);

    asm volatile(
        "cli\n\t"
        
        "movq $0, %%gs:0\n\t"
        "movq %0, %%gs:8\n\t"

        "mov %0, %%rsp\n\t"
        "push %1\n\t"
        "sti\n\t"
        "ret"
        :: "r"(krsp), "r"(sh)
        : "memory"
    );
    panic("System call exit failed");
}

void syscall_c(struct sysregs* args) {
    page_table_t* uasp = vmm_cpml4v();
    struct sysregs svargs;
    memcpy(&svargs, args, sizeof(*args));

    //printf("*** ENTER SYSCALL ***\nNUM: %p\nA0: %p A1: %p A2: %p\nA3: %p A4: %p A5: %p\n", args->num, args->a0, args->a1, args->a2, args->a3, args->a4, args->a5);
    
    switch (args->num) {
        case 1: 
            vmm_skasp();
            sys_exit(uasp);
        case 2: {
            args->num = read(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case 3: {
            args->num = write(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case 4: {
            args->num = open((char*)args->a0, args->a1);
            goto ret;
        }
        case 5: {
            args->num = close(args->a0);
            goto ret;
        }
        case 6: {
            if (open((char*)args->a0, O_CREAT) < 0) {
                args->num = -1;
                goto ret;
            } else {
                args->num = close(args->num);
                goto ret;
            }
        }
        case 7: {
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case 8: {
            args->num = chdir((char*)args->a0);
            goto ret;
        }
        case 9: {
            args->num = lseek(args->a0, args->a1, args->a2);
            goto ret;
        }
        case 10: {
            args->num = rename((char*)args->a0, (char*)args->a1);
            goto ret;
        }
        case 11: {
            args->num = mkdir((char*)args->a0);
            goto ret;
        }
        case 12: {
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case 13: {
            if (lai_acpi_reset() == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case 14: {
            args->num = stat((char*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case 15: {
            if (lai_enter_sleep(5) == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case 16: {
            rtc_sleep(args->a0);
            args->num = 0;
            goto ret;
        }
        case 17: {
            args->num = readdir((DIR*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case 18: {
            args->num = (u64)opendir((char*)args->a0);
            goto ret;
        }
        case 19: {
            args->num = closedir((DIR*)args->a0);
            goto ret;
        }
        case 20: {
            args->num = getcwd((char*)args->a0, args->a1);
            goto ret;
        }
        case 21: {
            args->num = sync(args->a0);
            goto ret;
        }
        case 22: {
            args->num = trunc(args->a0);
            goto ret;
        }
        case 23: {
            args->num = termctl(args->a0, args->a1);
            goto ret;
        }

        default: args->num = -1;
    }
ret: {
    // printf("\n*** EXIT SYSCALL ***\n");

    u64 ret = args->num;
    memcpy(args, &svargs, sizeof(*args));
    args->num = ret;
}
}