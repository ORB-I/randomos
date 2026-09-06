#pragma once

#include <sys/types.h>

usize strlen(const char* str);
s32 streq(const char* s1, const char* s2);
s32 strneq(const char* s1, const char* s2, usize n);
s32 atoi(const char* str);

void* memset(void *dest, int val, usize count);
void* memcpy(void* dest, const void* src, usize count);
int memcmp(const void* s1, const void* s2, usize n);
void* memmove(void* dst, const void* src, usize n);
void* memchr(const void* ptr, int c, usize n);

char* strchr(const char* str, char c);
char* strdup(char* str);
char* strtok(char* str, const char* delim);

int strtoi(char* ptr, char** eptr);
unsigned long strtoul(const char *nptr, char **endptr, int base);
usize strcspn(const char* s, const char* rj);
char* strcat(char* dest, const char* src);

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, usize n);
char* strncpy(char* dst, const char* src, usize sz);
char* strcpy(char* dst, const char* src);
long strtol(const char* nptr, char** endptr, int base);
char* strpbrk(const char* s, const char* cs);
usize strspn(const char* s, const char* cs);
usize strnlen(const char *s, usize maxlen);
usize strlcpy(char* dst, const char* src, usize size);