#pragma once
#include <core/std.h>

void ps2_wait_write();
void ps2_wait_read();

u8 ps2_dataread();
u8 ps2_datareadto(u16 to);
void ps2_datawrite(u8 data);
u8 ps2_statread();
u8 ps2_statreadto(u16 to);
void ps2_cmdwrite(u8 cmd);
int ps2_ackcmd(u8 cmd);
int isps2dc();