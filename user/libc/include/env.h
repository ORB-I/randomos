#pragma once

char* getenv(char* name);
int setenv(char* name, char* val, int overwrite);

char** __libc_getenviron();
#define environ (__libc_getenviron())