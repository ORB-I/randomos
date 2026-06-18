#include <core/idt.h>
#include <core/panic.h>

#include <drivers/vga.h>
#include <drivers/fs.h>
#include <drivers/rtc.h>

#include <lib/sh.h>
#include <lib/loader.h>

#include <lai/helpers/pm.h>

extern void syscall_s();
extern u8* kern_stack;

void init_syscalls() {
    idt_regintr(0x80, syscall_s, 0xEE);
}

struct sysregs {
    u32 edi, esi, ebp, kernel_esp, 
        ebx, edx, ecx, eax;
    
    u32 user_eip, user_cs, 
        user_eflags, user_esp, 
        user_ss;
};

[[noreturn]] void sys_exit() {
    u32 kesp = (u32)kern_stack + 4096;
    asm volatile(
        "mov %0, %%esp\n\t"
        "push %1\n\t"
        "ret"
        :: "r"(kesp), "r"(sh)
        : "memory"
    );

    panic("System call EXIT failed");
}

void syscall_c(struct sysregs* args) {
    switch (args->eax) {
        case 1: sys_exit();
        case 2: {
            args->eax = read(args->ebx, (u8*)args->ecx, args->edx);
            return;
        }
        case 3: {
            args->eax = write(args->ebx, (u8*)args->ecx, args->edx);
            return;
        }
        case 4: {
            args->eax = open((char*)args->ebx, args->ecx);
            return;
        }
        case 5: {
            args->eax = close(args->ebx);
            return;
        }
        case 6: {
            if (open((char*)args->ebx, O_CREAT) < 0) {
                args->eax = -1;
                return;
            } else {
                args->eax = close(args->eax);
                return;
            }
        }
        case 7: {
            args->eax = unlink((char*)args->ebx);
            return;
        }
        case 8: {
            args->eax = chdir((char*)args->ebx);
            return;
        }
        case 9: {
            args->eax = lseek(args->ebx, args->ecx, args->edx);
            return;
        }
        case 10: {
            args->eax = rename((char*)args->ebx, (char*)args->ecx);
            return;
        }
        case 11: {
            args->eax = mkdir((char*)args->ebx);
            return;
        }
        case 12: {
            args->eax = unlink((char*)args->ebx);
            return;
        }
        case 13: {
            if (lai_acpi_reset() == 0) args->eax = 0;
            else args->eax = -1;
            return;
        }
        case 14: {
            args->eax = stat((char*)args->ebx, (struct stat*)args->ecx);
            return;
        }
        case 15: {
            if (lai_enter_sleep(5) == 0) args->eax = 0;
            else args->eax = -1;
            return;
        }
        case 16: {
            rtc_sleep(args->ebx);
            args->eax = 0;
            return;
        }
        case 17: {
            args->eax = readdir((DIR*)args->ebx, (struct stat*)args->ecx);
            return;
        }
        case 18: {
            args->eax = (u32)opendir((char*)args->ebx);
            return;
        }
        case 19: {
            args->eax = closedir((DIR*)args->ebx);
            return;
        }
        case 20: {
            args->eax = getcwd((char*)args->ebx, args->ecx);
            return;
        }
        case 21: {
            args->eax = sync(args->ebx);
            return;
        }
        case 22: {
            args->eax = trunc(args->ebx);
            return;
        }
        case 23: {
            args->eax = termctl(args->ebx, args->ecx);
        }

        default: args->eax = -1;
    }
}