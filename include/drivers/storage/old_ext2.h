#pragma once
#include <core/std.h>
#include <drivers/storage/fs.h>

/* read-only ext2 backend sitting on the same block layer fatfs uses.
   every value coming off the disk gets range-checked before use so a
   corrupt image fails a syscall, not the kernel */

int ext2_detect(void);          /* probe drive superblock, 1 if ext2 */
int ext2_mount(const char* path);

int ext2_open(const char* path, int flags);
ssize ext2_read(int fd, void* buf, usize size);
off_t ext2_lseek(int fd, off_t off, int whence);
int ext2_opendir(const char* path);
int ext2_readdir(int cdp, struct stat* st);
int ext2_stat(const char* path, struct stat* st);
int ext2_chdir(const char* path);
int ext2_getcwd(char* buf, usize len);
