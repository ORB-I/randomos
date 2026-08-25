#pragma once
#include <core/std.h>

#define IPI_TRIGGER_EDGE 0x0
#define IPI_TRIGGER_LVL  0x1

#define IPI_LEVEL_DEASSERT 0x0
#define IPI_LEVEL_ASSERT   0x1

#define IPI_DSTMODE_PHYS 0x0
#define IPI_DSTMODE_LOG  0x1

#define IPI_DELMODE_FIXED 0x00
#define IPI_DELMODE_LOWP  0x01
#define IPI_DELMODE_SMI   0x02
#define IPI_DELMODE_RESV1 0x03
#define IPI_DELMODE_NMI   0x04
#define IPI_DELMODE_INIT  0x05
#define IPI_DELMODE_STUP  0x06
#define IPI_DELMODE_RESV2 0x07

#define IPI_SHRTDST_NONE 0x00
#define IPI_SHRTDST_SELF 0x01
#define IPI_SHRTDST_ALLI 0x02 // all including self
#define IPI_SHRTDST_ALLE 0x03 // all excluding self

extern volatile u32* lapic_virt_addr;
#define LAPIC_REG(offset) ((volatile uint32_t*)((uintptr_t)lapic_virt_addr + (offset)))

void ipi_send(u8 dest, u8 shrtdst, u8 trigger, u8 level, u8 dstmode, u8 delmode, u8 vec);
