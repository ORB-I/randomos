#pragma once
#include <sys/types.h>

#define O_WRONLY FA_WRITE
#define O_RDONLY FA_READ
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT FA_OPEN_ALWAYS
#define O_APPEND FA_OPEN_APPEND
#define O_TRUNC 0x04

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef s32 ssize;
typedef s32 off_t;

// internal kernel structure
// DO NOT ATTEMPT TO ACCESS 
// THE FIELDS WITHIN
typedef struct {
	u8	obj[40];
	u32	dptr;
	u32	clust;
	u32	sect;
	u8*	dir;
	u8	fn[12];
	u32	blk_ofs;
} DIR;

struct stat {
    char st_name[256];
    u8 st_attrib;
    usize st_size;
};

int open(char* path, u32 flags);
int close(int fd);
int creat(char* path);
int unlink(char* path);
int chdir(char* path);
off_t lseek(int fd, off_t off, u32 whence);
int rename(char* oldname, char* newname);
int mkdir(char* path);
int rmdir(char* path);
int stat(char* path, struct stat* st);
int readdir(DIR* dir, struct stat* st);
DIR* opendir(char* path);
int closedir(DIR* dir);
int getcwd(char* buf, usize buflen);
int sync(int fd);
int trunc(int fd);