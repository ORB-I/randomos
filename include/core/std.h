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