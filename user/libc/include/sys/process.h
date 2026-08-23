#pragma once

// blocks till a child dies and returns its pid, pass -1 to wait on any
// child instead of a specific one. -1 back means there was no such child.
int wait(int pid);

// terminates another process, 0 when it worked, -1 when the pid doesnt
// exist, is already dead or is the caller itself (use exit for that)
int kill(int pid);

int newproc(const char* path, char** argv);
int getpid();