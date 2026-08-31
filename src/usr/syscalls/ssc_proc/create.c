#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <lib/string.h>
#include <core/mem/pmm.h>
#include <core/mem/vmm.h>
#include <core/errno.h>
#include "../ssc.h"

void proc2ctx(procctx_t* dst, process_state_t* src);
extern int scheduler_execve;

struct copy_npa_res {
    int stat;
    u64 phys;
    usize npgs;
    char* kpath;
    char** kargv;
    char** kenvp;
};
#define COPY_NPA_BAD(ERRNO) ((struct copy_npa_res){ERRNO,0,0,NULL,NULL,NULL})

struct copy_npa_res copy_newprocargs(const char* path, char** argv, char** envp) {
    // we need to copy our args into the kasp and switch
    // cuz if we dont new_process calls load_program
    // which without switching will overwrite the current user program

    usize nargs = 0;
    usize nenv = 0;
    usize totalsz = sizeof(char*); // space for the NULL terminator

    while (nargs < 16 && argv[nargs] != NULL) { // 16 cuz thats ARGMAX
        totalsz += sizeof(char*) + strlen(argv[nargs]) + 1;
        nargs++;
    }

    while (envp[nenv] != NULL) {
        totalsz += sizeof(char*) + strlen(envp[nenv]) + 1;
        nenv++;
    }

    totalsz += strlen(path) + 1;
    
    usize npgs = (totalsz + 4095) / 4096;

    void* phys = pmm_falloc(npgs);
    if (!phys) return COPY_NPA_BAD(-ENOMEM);

    void* virt = (void*)((u64)phys + HHDM_START);
    char** kargv = (char**)virt;
    char** kenvp = (char**)(virt + sizeof(char*) * (nargs + 1));
    char* p = (char*)kenvp + sizeof(char*) * (nenv + 1);

    usize pathlen = strlen(path) + 1;

    char* kpath = p;

    memcpy(p, path, pathlen);
    p += pathlen;

    for (usize i = 0; i < nargs; i++) {
        usize len = strlen(argv[i]) + 1;
        kargv[i] = p;
        memcpy(p, argv[i], len);
        p += len;
    }
    kargv[nargs] = NULL;

    for (usize i = 0; i < nenv; i++) {
        usize len = strlen(envp[i]) + 1;
        kenvp[i] = p;
        memcpy(p, envp[i], len);
        p += len;
    }
    kenvp[nenv] = NULL;

    return (struct copy_npa_res){
        0,
        (u64)phys,
        npgs,
        kpath,
        kargv,
        kenvp
    };
}

//int sys_newproc(page_table_t* uasp, const char* path, char** argv, char** envp, u8 ppid) {
DEFSYSCALL(sys_newproc) {
    page_table_t* uasp = vmm_cpml4v();
    char* path = (char*)args->a0;
    char** argv = (char**)args->a1;
    char** envp = (char**)args->a2;
    if (!path || !argv || !envp) return -EINVAL;
    struct copy_npa_res res = copy_newprocargs(path, argv, envp);
    if (res.stat < 0) {
        return res.stat;
    }

    vmm_skasp();
    int ret = new_process(res.kpath, res.kargv, res.kenvp, current_pid);
    vmm_sasp(uasp);

    pmm_ffree((void*)res.phys, res.npgs);

    return ret;
}

DEFSYSCALL(sys_execve) {
    page_table_t* uasp = vmm_cpml4v();
    char* path = (char*)args->a0;
    char** argv = (char**)args->a1;
    char** envp = (char**)args->a2;
    if (!path || !argv || !envp) return -EINVAL;

    struct copy_npa_res res = copy_newprocargs(path, argv, envp);
    if (res.stat < 0) return res.stat;

    vmm_skasp();
    int ret = kexecve(res.kpath, res.kargv, res.kenvp, current_pid);
    vmm_sasp(uasp);

    pmm_ffree((void*)res.phys, res.npgs);
    if (ret < 0) return ret;

    procctx_t ctx;
    proc2ctx(&ctx, &proctbl[current_pid]);
    scheduler_execve = 1;
    scheduler_switch(&ctx);
    return 0;
}