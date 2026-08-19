#pragma once
#include <core/std.h>

void ps2_wait_write();
void ps2_wait_read();

u8 ps2_dataread();
void ps2_datawrite(u8 data);
u8 ps2_statread();
void ps2_cmdwrite(u8 cmd);