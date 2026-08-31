#pragma once
#include <core/std.h>
#include <core/errno.h>

int ensure_pointer(void* uptr, usize sz, int write);
int ensure_string(char* str, usize maxsz, int write);