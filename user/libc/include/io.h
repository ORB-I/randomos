#pragma once

#include <stdarg.h>

void fputchar(int fd, char c);
void putchar(char c);

void vfprintf(int fd, const char* fmt, va_list lst);
void printf(const char* fmt, ...);
void fprintf(int fd, const char* fmt, ...);
