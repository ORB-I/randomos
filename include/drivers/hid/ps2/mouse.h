#pragma once
#include <core/std.h>

void init_mouseps2();
int has_ps2mouse();

void enqueue_mouse(mouse_info_t info);
mouse_info_t dequeue_mouse();

int get_mouse_info(mouse_info_t* buf);
