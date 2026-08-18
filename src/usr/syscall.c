#include <core/std.h>
#include <core/idt.h>
#include <core/panic.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>

#include <drivers/gettimeofday.h>
#include <drivers/term.h>
#include <drivers/fs.h>
#include <drivers/fb.h>
#include <drivers/tsc.h>

#include <lib/sh.h>
#include <lib/loader.h>
#include <lib/syscall.h>

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
    vmm_remumap(uasp);
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
    
    switch (args->num) {
        case SYS_EXIT: 
            vmm_skasp();
            sys_exit(uasp);
        case SYS_READ: {
            args->num = read(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case SYS_WRITE: {
            args->num = write(args->a0, (u8*)args->a1, args->a2);
            goto ret;
        }
        case SYS_OPEN: {
            args->num = open((char*)args->a0, args->a1);
            goto ret;
        }
        case SYS_CLOSE: {
            args->num = close(args->a0);
            goto ret;
        }
        case SYS_CREAT: {
            int fd;
            if ((fd = open((char*)args->a0, O_CREAT)) < 0) {
                args->num = -1;
                goto ret;
            } else {
                args->num = close(fd);
                goto ret;
            }
        }
        case SYS_UNLINK: {
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case SYS_CHDIR: {
            args->num = chdir((char*)args->a0);
            goto ret;
        }
        case SYS_LSEEK: {
            args->num = lseek(args->a0, args->a1, args->a2);
            goto ret;
        }
        case SYS_RENAME: {
            args->num = rename((char*)args->a0, (char*)args->a1);
            goto ret;
        }
        case SYS_MKDIR: {
            args->num = mkdir((char*)args->a0);
            goto ret;
        }
        case SYS_RMDIR: {
            args->num = unlink((char*)args->a0);
            goto ret;
        }
        case SYS_REBOOT: {
            if (lai_acpi_reset() == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case SYS_STAT: {
            args->num = stat((char*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case SYS_POWEROFF: {
            if (lai_enter_sleep(5) == 0) args->num = 0;
            else args->num = -1;
            goto ret;
        }
        case SYS_SLEEP: {
            tsc_sleep(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_READDIR: {
            args->num = readdir((DIR*)args->a0, (struct stat*)args->a1);
            goto ret;
        }
        case SYS_OPENDIR: {
            args->num = (u64)opendir((char*)args->a0);
            goto ret;
        }
        case SYS_CLOSEDIR: {
            args->num = closedir((DIR*)args->a0);
            goto ret;
        }
        case SYS_GETCWD: {
            args->num = getcwd((char*)args->a0, args->a1);
            goto ret;
        }
        case SYS_SYNC: {
            args->num = sync(args->a0);
            goto ret;
        }
        case SYS_TRUNC: {
            args->num = trunc(args->a0);
            goto ret;
        }
        case SYS_TERMCTL: {
            args->num = termctl(args->a0, args->a1);
            goto ret;
        }
        case SYS_CREATEFB: {
            args->num = create_fb(args->a0);
            goto ret;
        }
        case SYS_RMFB: {
            free_fb(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_SWITCHFB: {
            args->num = switch_fb(args->a0);
            goto ret;
        }
        case SYS_CLEARFB: {
            clear_fb(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_FLUSHSCR: {
            flush_scr();
            args->num = 0;
            goto ret;
        }
        case SYS_GETFBINF: {
            args->num = get_fbinfo(args->a0, (framebuf_info_t*)args->a1);
            goto ret;
        }
        case SYS_GETFBTYP: {
            args->num = get_typefb(args->a0);
            goto ret;
        }
        case SYS_GETCURFB: {
            args->num = get_currfb();
            goto ret;
        }
        case SYS_GETTIMEOFDAY: {
            args->num = gettimeofday();
            goto ret;
        }
        case SYS_GETMTIMEOFDAY: {
            getmtimeofday((struct millitime*)args->a0);
            goto ret;
        }
        case SYS_GETTIMEMONOMS: {
            args->num = get_tscms();
            goto ret;
        }
        case SYS_GETTIMEMONO: {
            args->num = rdtsc();
            goto ret;
        }
        case SYS_MMAP: {
            args->num = (u64)user_mmap(uasp, (void*)args->a0, args->a1);
            goto ret;
        }
        case SYS_MUNMAP: {
            args->num = user_munmap(uasp, (void*)args->a0, args->a1);
            goto ret;
        }

        default: args->num = -1;
    }
ret: {

    u64 ret = args->num;
    memcpy(args, &svargs, sizeof(*args));
    args->num = ret;
}
}