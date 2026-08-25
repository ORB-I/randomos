#pragma once

#include <core/std.h>
#include <arch/idt.h>

void idt_init();
void idt_regintr(idt_entry_t* idt, u8 vector, void* isr, u8 flags, int ist);

// technically mem/gdt but like whatever
void reset_rsp(u64 addr);