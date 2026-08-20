#pragma once
#include <core/std.h>

#define MAX_PROCESSES 255
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
extern u8 nprocs;
extern u8 current_pid;
int new_process(const char* path, char** argv, u8 ppid);
