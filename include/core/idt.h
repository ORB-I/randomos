#pragma once

#include <core/std.h>

void idt_init();
void idt_regintr(u8 vector, void* isr, u8 flags, int ist);

// technically mem/gdt but like whatever
void reset_rsp(u64 addr);