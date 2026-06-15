#pragma once

#include <core/std.h>

void pic_remap(s32 offset1, s32 offset2);
void pic_send_eoi(u8 irq);
void pic_disable();
void irq_disable(u8 line);
void irq_enable(u8 line);
void init_irq(s32 irq, void (*hdlr)());