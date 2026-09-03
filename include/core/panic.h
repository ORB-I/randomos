#pragma once
#include <core/std.h>

__noreturn __no_protect void panic(const char* msg, ...) __printf(1, 2);