#pragma once

#include <sys/types.h>

usize strlen(const char* str);
s32 streq(const char* s1, const char* s2);
s32 strneq(const char* s1, const char* s2, usize n);
s32 atoi(const char* str);

void* memset(void *dest, int val, usize count);
void* memcpy(void* dest, const void* src, usize count);
int memcmp(const void* s1, const void* s2, usize n);

char* strchr(const char* str, char c);