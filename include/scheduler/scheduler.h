#pragma once

#include <scheduler/process.h>

int init_scheduler();
[[noreturn]] void start_scheduler();
void scheduler_switch(procctx_t* ctx);

// set by the preempt timer when a switch has to be deferred until the
// running syscall finishes; consumed on the way back to user mode
extern u8 preempt_pending;
extern procctx_t preempt_ctx;
