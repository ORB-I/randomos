; packed procctx_t in include/scheduler/process.h
%define CTX_RIP       0x00
%define CTX_RSP       0x08
%define CTX_RFLAGS    0x10

%define CTX_RAX       0x18
%define CTX_RBX       0x20
%define CTX_RCX       0x28
%define CTX_RDX       0x30
%define CTX_RSI       0x38
%define CTX_RDI       0x40
%define CTX_RBP       0x48
%define CTX_R8        0x50
%define CTX_R9        0x58
%define CTX_R10       0x60
%define CTX_R11       0x68
%define CTX_R12       0x70
%define CTX_R13       0x78
%define CTX_R14       0x80
%define CTX_R15       0x88

%define CTX_CS        0x90
%define CTX_SS        0x92
%define CTX_FS        0x94
%define CTX_GS        0x96

%define CTX_FSB       0x98
%define CTX_GSB       0xA0
%define CTX_CR3       0xA8

%define CTX_SIZE      0xB0

; NT, IOPL, VM: iretq in long mode #GPs if NT is set, and we never
; want user code coming back with IOPL high enough to run cli.
%define RFLAGS_DROP   ((3 << 12) | (1 << 14) | (1 << 17))
