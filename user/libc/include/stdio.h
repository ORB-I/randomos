#pragma once
#include <stddef.h>

#define FILE_READ   0x01
#define FILE_WRITE  0x02
#define FILE_EOF    0x04
#define FILE_ERR    0x08
#define FILE_DIRTY  0x10
#define FILE_BUFSZ  4096
typedef struct {
    int fd;

    unsigned char* buf;
    size_t bufsz;
    size_t bufpos;
    size_t buflen;

    int flags;

    int ungotc;
    int has_ungotc;
} FILE;

#define BUFSIZ FILE_BUFSZ

extern FILE* __libc_stdout__;
extern FILE* __libc_stdin__;
extern FILE* __libc_stderr__;

#define stdout __libc_stdout__
#define stdin __libc_stdin__
#define stderr __libc_stderr__

#ifndef SEEK_SET
#    define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#    define SEEK_CUR 1
#endif
#ifndef SEEK_END
#    define SEEK_END 2
#endif

#define EOF (-1)

FILE* fopen(const char* path, const char* mode);
size_t fread(void* ptr, size_t sz, size_t nmemb, FILE* f);
size_t fwrite(const void* ptr, size_t sz, size_t nmemb, FILE* f);
int fflush(FILE* f);
int fclose(FILE* f);
long long ftell(FILE* f);
int fseek(FILE* f, long off, int whence);
int feof(FILE* f);
int ferror(FILE* f);
int fputs(const char* str, FILE* f);
int fputc(int c, FILE* f);
int fgetc(FILE* stream);
char* fgets(char* s, int n, FILE* f);
int remove(const char* path);
int ungetc(int c, FILE* stream);
int fileno(FILE* f);