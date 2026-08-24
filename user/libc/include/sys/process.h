#pragma once
#include <sys/types.h>

// blocks till a child dies and returns its pid, pass -1 to wait on any
// child instead of a specific one. -1 back means there was no such child.
int wait(int pid, int* code);

// terminates another process, 0 when it worked, -1 when the pid doesnt
// exist, is already dead or is the caller itself (use exit for that)
int kill(int pid);

int newproc(const char* path, char** argv, char** envp);
int getpid();

uid_t getuid(void);
int setuid(uid_t uid);

gid_t getgid(void);
int setgid(gid_t gid);

uid_t geteuid(void);
int seteuid(uid_t euid);

gid_t getegid(void);
int setegid(gid_t egid);
int execve(char* path, char** argv, char** envp);