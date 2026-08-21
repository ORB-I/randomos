#pragma once
#include <core/std.h>
#include <core/mem/vmm.h>

typedef struct {
    int status;
    page_table_t* pgtbl;
    u64 entry;
    u64 rsp;
} loadprog_res_t;
#define LOADPROG_ERR ((loadprog_res_t){-1,NULL,0,0})

loadprog_res_t load_program(const char* path, char** argv);
void reset_kgsb();
