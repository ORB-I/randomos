#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u64 usize;
typedef s64 ssize;

typedef u32 uid_t;
typedef u32 gid_t;

typedef __builtin_va_list va_list;
#define va_start(lst, ap) __builtin_va_start(lst, ap)
#define va_end(lst) __builtin_va_end(lst)
#define va_arg(lst, type) __builtin_va_arg(lst, type)
#define va_copy(dst, src) __builtin_va_copy(dst, src)

#define __maybe_unused __attribute__((unused))
#define __noreturn __attribute__((noreturn))
#define __printf(fmt, st) __attribute__((format(printf, fmt, st)))
#define __packed __attribute__((packed))
#define __section(name) __attribute__((section(#name)))
#define __always_emit __attribute__((used))
#define __align(x) __attribute__((aligned(x)))
#define __no_protect __attribute__((no_stack_protector))
#define __deprecated(msg) __attribute__((deprecated(msg)))
#define __must_inline __attribute__((always_inline))