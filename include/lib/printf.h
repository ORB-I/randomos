#pragma once

#include <core/std.h>

typedef void (*writer_t)(const char* str, usize len);
void vwprintf(writer_t wr, const char* fmt, va_list lst);