#pragma once

void init_cmdline();
int cmdline_has(const char* name);
const char* cmdline_get(const char* name);