#include <core/idt.h>
#include <core/panic.h>

#include <drivers/vga.h>
#include <drivers/fs.h>

#include <lib/sh.h>

extern void syscall_s();
extern u8* kern_stack;

#define URAM_START 0x00400000

void init_syscalls() {
    idt_regintr(0x80, syscall_s, 0x8E);
}

struct sysregs {
    u32 edi, esi, ebp, kernel_esp, 
        ebx, edx, ecx, eax;
    
    u32 user_eip, user_cs, 
        user_eflags, user_esp, 
        user_ss;
};

[[noreturn]] void sys_exit() {
    u8* uram = (u8*)URAM_START;
    for (u32 i = 0; i < (512 * 1024 * 1024); i++) uram[i] = 0;
    
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
        case 4: {
            u8* buf = (u8*)(URAM_START + args->ecx);
            usize sz = args->edx;
            switch (args->ebx) {
                case 1:
                    for (usize i = 0; i < sz; i++) {
                        vga_putchar(buf[i]);
                    }
                    args->eax = sz;
                    break;
                default:
                    args->eax = write(args->ebx, buf, sz);
            }
            return;
        }
        default: args->eax = -1;
    }
}