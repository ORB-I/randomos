#include <core/std.h>
#include <core/idt.h>
#include <core/mem/vmm.h>
#include <core/asmh.h>
#include <core/panic.h>

#include <drivers/time/gettimeofday.h>
#include <drivers/term.h>
#include <drivers/storage/fs.h>
#include <drivers/hid/mouse.h>
#include <drivers/fb.h>
#include <drivers/time/clock.h>
#include <drivers/hid/kbd.h>

#include <lib/loader.h>
#include <lib/syscall.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <lib/loader.h>

#include <lai/helpers/pm.h>

#define MSR_EFER          0xC0000080
#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_USER_GS_BASE 0xC0000101

extern void syscall_s();

void init_syscalls() {
    reset_kgsb();
    wrmsr(MSR_LSTAR, (u64)syscall_s);
    wrmsr(MSR_STAR, ((u64)0x1B << 48) | ((u64)0x08 << 32));
    // mask DF so user std can't make kernel memcpy/memset run backwards.
    // IF stays unmasked: the HPET clock timer must keep firing for sleepms()
    // to work, and the preempt timer sets a flag that syscall_s checks on
    // exit. the critical sections (scheduler_switch calls) are guarded with
    // explicit cli/sti instead.
    wrmsr(MSR_SFMASK, (1 << 10));
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);
}

struct sysregs {
    u64 num, a0, a1, a2, a3, a4, a5;
    u64 __es, __ds, __rflags, __rip;
};

bool syscall_c(struct sysregs* args) {
    page_table_t* uasp = vmm_cpml4v();
    struct sysregs svargs;
    memcpy(&svargs, args, sizeof(*args));

    switch (args->num) {
        case SYS_EXIT: {
            vmm_skasp();
            proctbl[current_pid].is_dead = 1;

            // the running context is being abandoned, so what we hand
            // to the scheduler as "saved state" doesn't matter — it only
            // gets archived into a process that will never run again.
            // cli prevents the preempt timer from nesting into
            // scheduler_switch and corrupting the context copy.
            asm volatile("cli");
            procctx_t abandoned = {0};
            scheduler_switch(&abandoned);

            // scheduler_switch only returns when nobody is left
            panic("all processes have exited");
        }
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
            sleepms(args->a0);
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
            args->num = getms();
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
        case SYS_GETRAWSC: {
            args->num = kbd_get_raw();
            goto ret;
        }
        case SYS_CREATEFBWMEM: {
            args->num = create_fb_withmem(args->a0, (void*)args->a1, args->a2, (int*)args->a3);
            goto ret;
        }
        case SYS_RMFBWMEM: {
            free_fb_withmem(args->a0);
            args->num = 0;
            goto ret;
        }
        case SYS_GETMOUSEINFO: {
            args->num = get_mouse_info((mouse_info_t*)args->a0);
            goto ret;
        }
        case SYS_NEWPROC: {
            args->num = new_process((const char*)args->a0, (char**)args->a1, current_pid);
            goto ret;
        }
        default: args->num = -1;
    }
ret: {

    u64 ret = args->num;
    memcpy(args, &svargs, sizeof(*args));
    args->num = ret;
}
return false;
}
