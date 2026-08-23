#pragma once

#include <stdarg.h>
#include <sys/types.h>

void fputchar(int fd, char c);
void putchar(char c);
char* readline(const char* prompt);
void serial_write(void* buf, usize sz);

#include <printf.h>