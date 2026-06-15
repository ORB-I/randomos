#pragma once

#include <core/std.h>

void idt_init();
void idt_regintr(u8 vector, void* isr, u8 flags);