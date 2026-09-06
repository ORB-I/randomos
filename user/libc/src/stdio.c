#include <stdio.h>
#include <sys/sysfn.h>
#include <fs.h>
#include <str.h>
#include <mem.h>

FILE* __libc_stdout__ = NULL;
FILE* __libc_stdin__  = NULL;
FILE* __libc_stderr__ = NULL;

FILE* fopen(const char* path, const char* mode) {
    usize modelen = strlen(mode);
    int oflags = 0;
    int fflags = 0;

    if (modelen < 1) {
        return NULL;
    }

    switch (mode[0]) {
        case 'r': {
            oflags = O_RDONLY;
            fflags = FILE_READ;

            if (modelen >= 2 && mode[1] == '+') {
                oflags = O_RDWR;
                fflags = FILE_READ | FILE_WRITE;
            }
            break;
        }

        case 'w': {
            oflags = O_WRONLY | O_CREAT | O_TRUNC;
            fflags = FILE_WRITE;

            if (modelen >= 2 && mode[1] == '+') {
                oflags = O_RDWR | O_CREAT | O_TRUNC;
                fflags = FILE_READ | FILE_WRITE;
            }
            break;
        }

        case 'a': {
            oflags = O_WRONLY | O_CREAT | O_APPEND;
            fflags = FILE_WRITE;

            if (mode[1] == '+') {
                oflags = O_RDWR | O_CREAT | O_APPEND;
                fflags = FILE_READ | FILE_WRITE;
            }
            break;
        }

        default: return NULL;
    }

    int fd = open((char*)path, oflags, 0644);
    if (fd < 0) return NULL;

    FILE* f = malloc(sizeof(*f));
    if (!f) {
        close(fd);
        return NULL;
    }

    f->buf = malloc(FILE_BUFSZ);
    if (!f) {
        close(fd);
        free(f);
        return NULL;
    }

    f->fd = fd;
    f->bufsz = FILE_BUFSZ;
    f->bufpos = 0;
    f->buflen = 0;
    f->flags = fflags;

    return f;
}

size_t fread(void* ptr, size_t sz, size_t nmemb, FILE* f) {
    if (sz == 0 || nmemb == 0) return 0;
    if (!(f->flags & FILE_READ)) {
        f->flags |= FILE_ERR;
        return 0;
    }

    size_t total = sz * nmemb;
    u8* dst = ptr;
    size_t done = 0;

    while (done < total) {
        if (f->bufpos == f->buflen) {
            ssize n = read(f->fd, f->buf, f->bufsz);

            if (n < 0) {
                f->flags |= FILE_ERR;
                break;
            }

            if (n == 0) {
                f->flags |= FILE_EOF;
                break;
            }

            f->bufpos = 0;
            f->buflen = (size_t)n;
        }

        size_t avail = f->buflen - f->bufpos;
        size_t n = total - done;

        if (n > avail) n = avail;
        memcpy(dst + done, f->buf + f->bufpos, n);

        f->bufpos += n;
        done += n;
    }

    return done / sz;
}

size_t fwrite(const void* ptr, size_t sz, size_t nmemb, FILE* f) {
    if (sz == 0 || nmemb == 0) return 0;

    if (!(f->flags & FILE_WRITE)) {
        f->flags |= FILE_ERR;
        return 0;
    }

    const unsigned char* src = ptr;
    size_t total = sz * nmemb;
    size_t done = 0;

    while (done < total) {
        size_t space = f->bufsz - f->bufpos;

        if (space == 0) {
            if (fflush(f) < 0)
                break;

            space = f->bufsz;
        }

        size_t n = total - done;

        if (n > space) n = space;
        memcpy(f->buf + f->bufpos, src + done, n);

        f->bufpos += n;
        done += n;
        f->flags |= FILE_DIRTY;
    }

    return done / sz;
}

int fflush(FILE* f) {
    if (!f) return -1;
    if (f->flags & FILE_WRITE) {
        size_t written = 0;

        while (written < f->bufpos) {
            ssize n = write(f->fd, f->buf + written, f->bufpos - written);

            if (n < 0) {
                f->flags |= FILE_ERR;
                return -1;
            }

            if (n == 0) {
                f->flags |= FILE_ERR;
                return -1;
            }

            written += n;
        }

        f->bufpos = 0;
        f->flags &= ~FILE_DIRTY;
    }

    return 0;
}

int fclose(FILE* f) {
    if (!f) return -1;
    int ret = 0;

    if (f->flags & FILE_WRITE) {
        if (fflush(f) < 0) ret = -1;
    }

    if (close(f->fd) < 0) ret = -1;
    free(f->buf);
    free(f);

    return ret;
}

long long ftell(FILE* f) {
    off_t pos = lseek(f->fd, 0, SEEK_CUR);

    if (pos < 0) return -1;

    if (f->flags & FILE_READ) pos -= (off_t)(f->buflen - f->bufpos);
    else if (f->flags & FILE_WRITE) pos += (off_t)f->bufpos;

    return pos;
}

int fseek(FILE* f, long offset, int whence) {
    if (!f) return -1;

    if (f->flags & FILE_WRITE) {
        if (fflush(f) < 0) return -1;
    }

    if (f->flags & FILE_READ) {
        off_t correction = (off_t)(f->buflen - f->bufpos);
        if (whence == SEEK_CUR) offset -= correction;
    }

    off_t result = lseek(f->fd, offset, whence);

    if (result < 0) return -1;

    f->bufpos = 0;
    f->buflen = 0;
    f->flags &= ~FILE_EOF;

    return 0;
}

int feof(FILE* f) {
    return f->flags & FILE_EOF;
}

int ferror(FILE* f) {
    return f->flags & FILE_ERR;
}

int fputs(const char* str, FILE* f) {
    usize n = strlen(str);
    if (fwrite(str, n, 1, f) < 1) return -1;
    if (fwrite("\n", 1, 1, f) < 1) return -1;
    return 0;
}

int fputc(int c, FILE* stream) {
    if (stream->bufpos >= stream->bufsz) {
        if (fflush(stream) == EOF) return EOF;
    }
    stream->buf[stream->bufpos++] = (unsigned char)c;
    return (unsigned char)c;
}

int fgetc(FILE* stream) {
    if (stream->has_ungotc) {
        stream->has_ungotc = 0;
        return stream->ungotc;
    }

    if (stream->bufpos >= stream->buflen) {
        ssize n = read(stream->fd, stream->buf, stream->bufsz);
        if (n <= 0) return EOF;
        stream->bufpos = 0;
        stream->buflen = (size_t)n;
    }

    return stream->buf[stream->bufpos++];
}

char* fgets(char* s, int n, FILE* f) {
    if (n <= 0) return NULL;
    char *p = s;
    while (p < s + n - 1) {
        int c = fgetc(f);
        if (c == EOF) {
            if (p == s) return NULL;
            break;
        }
        *p++ = (char)c;
        if (c == '\n') break;
    }

    *p = '\0';
    return s;
}

int remove(const char* path) {
    if (unlink((char*)path) > 0) return -1;
    return 0;
}

int ungetc(int c, FILE* stream) {
    if (c == EOF || stream->has_ungotc) return EOF;
    stream->ungotc = (unsigned char)c;
    stream->has_ungotc = 1;
    return (unsigned char)c;
}

int fileno(FILE* f) {
    return f->fd;
}

void __libc_setupfail();

void __libc_init_stdioptr(FILE** ptr, int fd, int fflags) {
    *ptr = malloc(sizeof(FILE*));
    if (!*ptr) __libc_setupfail();
    (*ptr)->buf = malloc(FILE_BUFSZ);
    if (!(*ptr)->buf) {
        free(*ptr);
        __libc_setupfail();
    }

    (*ptr)->fd = fd;
    (*ptr)->bufsz = FILE_BUFSZ; 
    (*ptr)->bufpos = 0;
    (*ptr)->buflen = 0;
    (*ptr)->flags = fflags;
}

void __libc_fini_stdioptr(FILE** ptr) {
    fflush(*ptr);
    free((*ptr)->buf);
    free(*ptr);
}

void __libc_initstdio() {
    __libc_init_stdioptr(&__libc_stdout__, STDOUT, FILE_WRITE);
    __libc_init_stdioptr(&__libc_stderr__, STDERR, FILE_WRITE);
    __libc_init_stdioptr(&__libc_stdin__, STDIN, FILE_READ);
}

void __libc_finistdio() {
    __libc_fini_stdioptr(&__libc_stdout__);
    __libc_fini_stdioptr(&__libc_stderr__);
    __libc_fini_stdioptr(&__libc_stdin__);
}