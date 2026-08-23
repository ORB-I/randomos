#pragma once
#include <core/std.h>

#define MAX_PROCESSES 255
// passed as a wait target when any dead child will do
#define WAIT_ANY 0xFF
typedef struct {
    u64 rip;
    u64 rsp;
    u64 rflags;

    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi;
    u64 rbp;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;

    u16 cs, ss, fs, gs;
    u64 fsb, gsb;

    u64 cr3;

    u8 pid;
    u8 is_dead;
    u8 ppid;

    // set while parked in SYS_WAIT or SYS_SLEEP, the scheduler skips us and exit
    // clears this again when the child we asked for dies or sleep deadline passes
    u8 is_blocked;
    // child pid we are blocked on, WAIT_ANY when anyone dying is fine
    u8 wait_pid;
    // target time in ms to wake up if sleeping
    u64 wake_ms;

    u16 code;
    int* codeptr;

    uid_t uid;
    gid_t gid;
    uid_t euid;
    gid_t egid;

    u8 used; // process table entrry in use
} process_state_t;

typedef struct {
  u64 rip;
  u64 rsp;
  u64 rflags;

  u64 rax, rbx, rcx, rdx;
  u64 rsi, rdi;
  u64 rbp;
  u64 r8, r9, r10, r11, r12, r13, r14, r15;

  u16 cs, ss, fs, gs;
  u64 fsb, gsb;

  u64 cr3;
} __attribute__((packed)) procctx_t;

extern process_state_t proctbl[MAX_PROCESSES];
extern u8 current_pid;
int new_process(const char* path, char** argv, char** envp, u8 ppid);
void wake_waiter(u8 pid);
void reparent_children(u8 old_ppid);
