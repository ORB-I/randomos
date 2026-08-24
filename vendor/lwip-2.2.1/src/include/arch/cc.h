#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

/* Basic integer types */

typedef int8_t   s8_t;
typedef uint8_t  u8_t;

typedef int16_t  s16_t;
typedef uint16_t u16_t;

typedef int32_t  s32_t;
typedef uint32_t u32_t;

typedef int64_t  s64_t;
typedef uint64_t u64_t;

/* printf format helpers */

#define X8_F   "02x"
#define X16_F  "04x"
#define X32_F  "08x"
#define U16_F "%u"
#define S16_F "%d"
#define U32_F "%u"
#define S32_F "%d"
#define SZT_F "%p"

#define LWIP_DEBUG 1

/* Compiler */

#define LWIP_PLATFORM_ASSERT(x) \
    do {                         \
        if (!(x))                \
            __builtin_trap();    \
    } while (0)

#include <core/printf.h>
#define LWIP_PLATFORM_DIAG(args) serial_printf args;

/* Packed structures */

#define PACK_STRUCT_FIELD(x) x __attribute__((packed))

#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Alignment */

#define LWIP_ALIGNMENT 8

#endif