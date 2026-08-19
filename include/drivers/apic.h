#pragma once

#include <core/std.h>

void apic_init();
void lapic_eoi();
void ioapic_set_irq(u8 irq, u8 vector, u32 lapic_id, bool masked);
void ioapic_mask_irq(u8 irq);
void ioapic_unmask_irq(u8 irq);
void ioapic_route_gsi(u32 gsi, u8 vector, u32 lapic_id, u16 flags, bool masked);
u32 get_lapic_id();