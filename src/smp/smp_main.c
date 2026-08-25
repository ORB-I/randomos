#include <core/std.h>
#include <drivers/display/serial.h>
#include <core/printf.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <smp/smp.h>
#include <smp/bspreq.h>
#include <smp/apreq.h>
#include <smp/ipi.h>
#include <lib/string.h>

extern usize ncores;
extern thread_idt_t* tidts;
extern thread_gdt_t* tgdts;
extern smp_stack_t* smp_stacks;

u8 get_apicid() {
    return (*((volatile u32*)(lapic_virt_addr + 20))) >> 24;
}

void smp_main_finish(ssize i);
void smp_main(u64 apic_id) {
    ssize i = -1;
    for (i = 0; i < (ssize)ncores; i++) {
        if (smp_info[i].apicid == apic_id) {
            break;
        }
    }

    asm volatile(
        "cli\n\t"
        "lgdt %0\n\t"

        "pushq $0x08\n\t"
        "leaq .flush(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"

        ".flush:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"

        "mov $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        "mov %1, %%rsp\n\t"

        "call *%2\n\t"
        :: "m"(tgdts[i].gdtr), "r"(smp_stacks[i].stack + sizeof(smp_stacks[i].stack)), "c"(smp_main_finish), "D"(i)
        : "rax", "memory"
    );

    for (;;) {
        asm volatile("hlt");
    }
}

typedef struct {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
       
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} intctx_t;

void store_ctx(intctx_t* ctx, ssize i) {
    apstates[i].r15 = ctx->r15;
    apstates[i].r14 = ctx->r14;
    apstates[i].r13 = ctx->r13;
    apstates[i].r12 = ctx->r12;
    apstates[i].r11 = ctx->r11;
    apstates[i].r10 = ctx->r10;
    apstates[i].r9  = ctx->r9;
    apstates[i].r8  = ctx->r8;
    apstates[i].rbp = ctx->rbp;
    apstates[i].rdi = ctx->rdi;
    apstates[i].rsi = ctx->rsi;
    apstates[i].rdx = ctx->rdx;
    apstates[i].rcx = ctx->rcx;
    apstates[i].rbx = ctx->rbx;
    apstates[i].rax = ctx->rax;
    apstates[i].rip = ctx->rip;
    apstates[i].cs  = ctx->cs;
    apstates[i].rflags = ctx->rflags;
    apstates[i].rsp = ctx->rsp;
    apstates[i].ss  = ctx->ss;
}

void smp_mainloop(ssize i);
void clear_ctx(ssize i) {
    apstates[i].rreq.arg = NULL;
    apstates[i].rreq.fn = NULL;
    apstates[i].r15 = 0;
    apstates[i].r14 = 0;
    apstates[i].r13 = 0;
    apstates[i].r12 = 0;
    apstates[i].r11 = 0;
    apstates[i].r10 = 0;
    apstates[i].r9 = 0;
    apstates[i].r8 = 0;
    apstates[i].rbp = 0;
    apstates[i].rdi = 0;
    apstates[i].rsi = 0;
    apstates[i].rdx = 0;
    apstates[i].rcx = 0;
    apstates[i].rbx = 0;
    apstates[i].rax = 0;
    apstates[i].rip = (u64)(void*)smp_mainloop;
    apstates[i].cs = 0x08;
    apstates[i].ss = 0x10;
    apstates[i].rsp = (u64)(smp_stacks[i].stack + sizeof(smp_stacks[i].stack));
    apstates[i].rflags = 0x200202;
}

void smp_contloop(ssize i);
void smp_request_hdlr_c(intctx_t* ctx) {
    ap_req_t* req = NULL;
    u8 apicid = get_apicid();
    serial_printf("SMP %d received interrupted\n", apicid);
    
    usize i = 0;
    for (; i < ncores; i++) {
        if (apreqvec[i].apicid == apicid) {
            req = &apreqvec[i];
        }
    }

    //while (atomic_load(&apstates[i].lock));
    atomic_store(&apstates[i].lock, 1);

    switch (req->type) {
        case AP_REQ_RUN: {
            // if we arent in AP_WAITING we dont want to
            // do anything since it will really mess up our state
            serial_printf("AP %d received RUN request\n", apicid);
            if (apstates[i].state != AP_WAITING) {
                serial_printf("AP %d will not RUN\n", apicid);
                atomic_store(&apstates[i].lock, 0);
                smp_contloop(i);
            }
            ap_runreq_t* run = (ap_runreq_t*)req->data;
            apstates[i].rreq = *run;
            apstates[i].state = AP_START;
            atomic_store(&apreqvec[i].done, 1);
            atomic_store(&apstates[i].lock, 0);
            smp_contloop(i);
        }
        case AP_REQ_PAUSE: {
            serial_printf("AP %d received PAUSE request\n", apicid);
            apstates[i].state = AP_PAUSED;
            store_ctx(ctx, i);
            atomic_store(&apreqvec[i].done, 1);
            atomic_store(&apstates[i].lock, 0);
            smp_contloop(i);
        }
        case AP_REQ_CONT: {
            serial_printf("AP %d received CONT request\n", apicid);
            apstates[i].state = AP_RUNNING;
            atomic_store(&apreqvec[i].done, 1);
            atomic_store(&apstates[i].lock, 0);
        }
        case AP_REQ_STOP: {
            serial_printf("AP %d received STOP request\n", apicid);
            apstates[i].state = AP_WAITING;
            send_bsp_request(apicid, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WAITING);
            atomic_store(&apreqvec[i].done, 1);
            atomic_store(&apstates[i].lock, 0);
            smp_contloop(i);
        }
    }
}

void smp_main_finish(ssize i) {
    /* INIT leaves this core's lapic software-disabled, without enabling
       it the bsp request IPI below is silently dropped */
    apic_enable_current();
    asm volatile(
        "lidt %0\n\t"
        "sti" 
        :: "m"(tidts[i].idtr)
    );
    send_bsp_request(get_apicid(), BSP_REQ_SETSTAT, NULL, SMP_STATUS_WAITING);
    smp_mainloop(i);
}

void smp_mainloop(ssize i) {
    u64 apic_id = get_apicid();
    for (;;) {
        while (atomic_load(&apstates[i].lock));
        atomic_store(&apstates[i].lock, 1);
        switch (apstates[i].state) {
            case AP_WAITING:
            case AP_PAUSED: {
                atomic_store(&apstates[i].lock, 0);
                break;
            }
            case AP_RUNNING: {
                asm("cli");
                atomic_store(&apstates[i].lock, 0);
                asm volatile(
                    "mov %0, %%r15\n\t"
                    "mov 136(%%r15), %%rax\n\t"
                    "mov 128(%%r15), %%rbx\n\t"
                    "mov 120(%%r15), %%rcx\n\t"
                    "mov 104(%%r15), %%rsi\n\t"
                    "mov 96(%%r15), %%rdi\n\t"
                    "mov 88(%%r15), %%rbp\n\t"
                    "mov 80(%%r15), %%r8\n\t"
                    "mov 72(%%r15), %%r9\n\t"
                    "mov 64(%%r15), %%r10\n\t"
                    "mov 56(%%r15), %%r11\n\t"
                    "mov 48(%%r15), %%r12\n\t"
                    "mov 40(%%r15), %%r13\n\t"
                    "mov 32(%%r15), %%r14\n\t"

                    "pushq 176(%%r15)\n\t"
                    "pushq 168(%%r15)\n\t"
                    "pushq 160(%%r15)\n\t"
                    "pushq 152(%%r15)\n\t"
                    "pushq 144(%%r15)\n\t"

                    "mov 24(%%r15), %%r15\n\t"
                    "iretq"
                    :: "m"(apstates[i])
                );
            }
            case AP_START: {
                serial_printf("AP %d START\n");
                asm("cli");

                void(*fn)(void*) = apstates[i].rreq.fn;
                void* arg = apstates[i].rreq.arg;
                clear_ctx(i);
                send_bsp_request(apic_id, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WORKING);
                atomic_store(&apstates[i].lock, 0);

                asm("sti");
                fn(arg);
                asm("cli");

                while (atomic_load(&apstates[i].lock));
                atomic_store(&apstates[i].lock, 1);

                clear_ctx(i);
                apstates[i].state = AP_WAITING;
                send_bsp_request(apic_id, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WAITING);
                atomic_store(&apstates[i].lock, 0);

                asm("sti");
                continue;
            }
        }
    }
}

[[noreturn]] void smp_contloop(ssize i) {
    asm volatile(
        "pushq $0x10\n\t"
        "pushq %0\n\t"
        "pushq $0x200202\n\t"
        "pushq $0x08\n\t"
        "pushq %1\n\t"
        "iretq"
        :: "r"(smp_stacks[i].stack + sizeof(smp_stacks[i].stack)),
           "r"(smp_mainloop)
        : "memory"
    );

    __builtin_unreachable();
}
