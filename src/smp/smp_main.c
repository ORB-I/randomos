#include <stddef.h>
#include <core/std.h>
#include <drivers/display/serial.h>
#include <core/kprint.h>
#include <arch/gdt.h>
#include <arch/idt.h>
#include <smp/smp.h>
#include <smp/bspreq.h>
#include <smp/apreq.h>
#include <smp/ipi.h>
#include <lib/string.h>
#include <core/errno.h>

extern usize ncores;
extern thread_idt_t* tidts;
extern thread_gdt_t* tgdts;
extern smp_stack_t* smp_stacks;

static void smp_mainloop(ssize i);
[[noreturn]] static void smp_contloop(ssize i);
static void smp_main_finish(ssize i);

/* what the request handler gets handed from asm: pushaq stashes the gp
   regs right below the hardware iret frame, so one struct laid out in
   push order covers the whole thing */
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

static ssize smp_find_core(u8 apicid) {
    for (ssize i = 0; i < (ssize)ncores; i++) {
        if (smp_info[i].apicid == apicid) return i;
    }
    return -ENOCORE;
}

void smp_main(u64 apic_id) {
    ssize i = smp_find_core((u8)apic_id);
    if (i < 0) {
        // a core the madt never told us about has no stack or gdt to go back to
        kprint("SMP: apicid %lu is not ours, halting\n", apic_id);
        for (;;) asm volatile("cli\n\thlt");
    }

    // far-return through the new gdt so cs actually reloads before we
    // touch any data segments
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

/* r15..ss sit contiguous and identically ordered in both structs, so a
   saved frame moves as one flat block */
_Static_assert(sizeof(intctx_t) == offsetof(intctx_t, ss) + sizeof(u64),
               "intctx_t grew padding");
_Static_assert(offsetof(ap_state, r15) + sizeof(intctx_t)
               == offsetof(ap_state, ss) + sizeof(u64),
               "ap_state reg block drifted");

static void store_ctx(intctx_t* ctx, ssize i) {
    memcpy(&apstates[i].r15, &ctx->r15, sizeof(intctx_t));
}

static void clear_ctx(ssize i) {
    memset(&apstates[i].r15, 0, sizeof(intctx_t));
    apstates[i].rreq.fn = NULL;
    apstates[i].rreq.arg = NULL;
    // fresh frame aimed back into the idle loop, this doubles as where
    // a resumed core lands once its function returns
    apstates[i].rip = (u64)(void*)smp_mainloop;
    apstates[i].cs = 0x08;
    apstates[i].ss = 0x10;
    apstates[i].rsp = (u64)(smp_stacks[i].stack + sizeof(smp_stacks[i].stack));
    apstates[i].rflags = 0x200202;
}

/* runs on whichever AP took the request IPI. most paths leave through
   smp_contloop and never come back through the asm epilogue, so the eoi
   has to happen here or the vector stays in-service and every later
   request on it gets swallowed */
void smp_request_hdlr_c(intctx_t* ctx) {
    kprint("AP received request\n");
    lapic_eoi();

    u8 apicid = get_apicid();
    ssize i = smp_find_core(apicid);
    if (i < 0) {
        kprint("SMP: request for untracked apicid %d\n", apicid);
        return;
    }
    ap_req_t* req = &apreqvec[i];
    u64 rflags;

    switch (req->type) {
        case AP_REQ_RUN: {
            kprint("AP %d received RUN request\n", apicid);
            // only safe to launch out of the idle loop, yanking a paused
            // or running core would orphan whatever frame it was wearing
            if (apstates[i].state != AP_WAITING) {
                kprint("AP %d will not RUN\n", apicid);
                atomic_store(&req->done, 1); // ack anyway or the bsp spins forever
                smp_contloop(i);
            }
            ap_runreq_t* run = (ap_runreq_t*)req->data;
            lock_acquire(&apstates[i].lock, &rflags);
            apstates[i].rreq = *run;
            apstates[i].state = AP_START;
            lock_release(&apstates[i].lock, &rflags);
            atomic_store(&req->done, 1);
            smp_contloop(i);
        }
        case AP_REQ_PAUSE: {
            kprint("AP %d received PAUSE request\n", apicid);
            lock_acquire(&apstates[i].lock, &rflags);
            apstates[i].state = AP_PAUSED;
            store_ctx(ctx, i);
            lock_release(&apstates[i].lock, &rflags);
            atomic_store(&req->done, 1);
            smp_contloop(i);
        }
        case AP_REQ_CONT: {
            kprint("AP %d received CONT request\n", apicid);
            lock_acquire(&apstates[i].lock, &rflags);
            apstates[i].state = AP_RUNNING;
            lock_release(&apstates[i].lock, &rflags);
            atomic_store(&req->done, 1);
            break;
        }
        case AP_REQ_STOP: {
            kprint("AP %d received STOP request\n", apicid);
            lock_acquire(&apstates[i].lock, &rflags);
            apstates[i].state = AP_WAITING;
            lock_release(&apstates[i].lock, &rflags);
            send_bsp_request(apicid, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WAITING);
            atomic_store(&req->done, 1);
            smp_contloop(i);
        }
    }
}

static void smp_main_finish(ssize i) {
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
        u64 rflags;
        lock_acquire(&apstates[i].lock, &rflags);
        switch (apstates[i].state) {
            case AP_WAITING:
            case AP_PAUSED: {
                lock_release(&apstates[i].lock, &rflags);
                break;
            }
            case AP_RUNNING: {
                // we need to copy the current state before we release the lock
                // since an interrupt could fire mid-switch and change the state
                // causing a broken state, and we cant keep the lock acquired until
                // iretq since calling lock_release would clobber our new register states
                ap_state state;
                memcpy(&state, &apstates[i], sizeof(state));
                lock_release(&apstates[i].lock, &rflags);
                asm volatile(
                    "mov %0, %%r15\n\t"
                    "mov %c1(%%r15), %%rax\n\t"
                    "mov %c2(%%r15), %%rbx\n\t"
                    "mov %c3(%%r15), %%rcx\n\t"
                    "mov %c4(%%r15), %%rdx\n\t"
                    "mov %c5(%%r15), %%rsi\n\t"
                    "mov %c6(%%r15), %%rdi\n\t"
                    "mov %c7(%%r15), %%rbp\n\t"
                    "mov %c8(%%r15), %%r8\n\t"
                    "mov %c9(%%r15), %%r9\n\t"
                    "mov %c10(%%r15), %%r10\n\t"
                    "mov %c11(%%r15), %%r11\n\t"
                    "mov %c12(%%r15), %%r12\n\t"
                    "mov %c13(%%r15), %%r13\n\t"
                    "mov %c14(%%r15), %%r14\n\t"

                    "pushq %c19(%%r15)\n\t" // ss
                    "pushq %c18(%%r15)\n\t" // rsp
                    "pushq %c17(%%r15)\n\t" // rflags
                    "pushq %c16(%%r15)\n\t" // cs
                    "pushq %c15(%%r15)\n\t" // rip

                    "mov %c20(%%r15), %%r15\n\t"
                    "iretq"
                    :: "r"(&state),
                       "i"(offsetof(ap_state, rax)),
                       "i"(offsetof(ap_state, rbx)),
                       "i"(offsetof(ap_state, rcx)),
                       "i"(offsetof(ap_state, rdx)),
                       "i"(offsetof(ap_state, rsi)),
                       "i"(offsetof(ap_state, rdi)),
                       "i"(offsetof(ap_state, rbp)),
                       "i"(offsetof(ap_state, r8)),
                       "i"(offsetof(ap_state, r9)),
                       "i"(offsetof(ap_state, r10)),
                       "i"(offsetof(ap_state, r11)),
                       "i"(offsetof(ap_state, r12)),
                       "i"(offsetof(ap_state, r13)),
                       "i"(offsetof(ap_state, r14)),
                       "i"(offsetof(ap_state, rip)),
                       "i"(offsetof(ap_state, cs)),
                       "i"(offsetof(ap_state, rflags)),
                       "i"(offsetof(ap_state, rsp)),
                       "i"(offsetof(ap_state, ss)),
                       "i"(offsetof(ap_state, r15))
                    : "memory");
                __builtin_unreachable();
            }
            case AP_START: {
                kprint("AP %d START\n", get_apicid());
                asm("cli");

                void(*fn)(void*) = apstates[i].rreq.fn;
                void* arg = apstates[i].rreq.arg;
                clear_ctx(i);
                send_bsp_request(apic_id, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WORKING);
                lock_release(&apstates[i].lock, &rflags);

                asm("sti");
                fn(arg);
                asm("cli");

                lock_acquire(&apstates[i].lock, &rflags);

                clear_ctx(i);
                apstates[i].state = AP_WAITING;
                send_bsp_request(apic_id, BSP_REQ_SETSTAT, NULL, SMP_STATUS_WAITING);
                lock_release(&apstates[i].lock, &rflags);

                asm("sti");
                continue;
            }
        }
    }
}

[[noreturn]] static void smp_contloop(ssize i) {
    // we need to call the mainloop through an iretq frame
    // once the interrupt handler is done because otherwise
    // its not garunteed that it would ever reach the main loop
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
