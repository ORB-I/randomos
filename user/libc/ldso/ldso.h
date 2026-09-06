#pragma once
#include <sys/types.h>
#include <sys/elf.h>

#define HIDDEN __attribute__((visibility("hidden")))
#define ASMFUNC __attribute((naked))
#define NORETURN __attribute__((noreturn))

#define O_WRONLY 0x01
#define O_RDONLY 0x02
#define O_RDWR (O_WRONLY | O_RDONLY)
#define O_CREAT 0x04
#define O_APPEND 0x08
#define O_TRUNC 0x10

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITE    (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE     (1ULL << 7)
#define PAGE_PWT      (1ULL << 3)
#define PAGE_PCD      (1ULL << 4)
#define PAGE_UNCACHE  (PAGE_PCD | PAGE_PWT)

#define MAP_ANYPHYS   (1ULL << 60)
#define MAP_CONT      (1ULL << 61)
#define MAP_ANYVIRT   (1ULL << 62)
#define MAP_USRMAP    (1ULL << 63)

#define UNMAP_KEEPPHYS (1ULL << 0)

typedef enum {
    OBJ_LOADED,
    OBJ_PREINITED,
    OBJ_INITED,
    OBJ_FINIED
} objstate_t;

#define MAXOBJS 128
typedef struct ObjectT object_t;
struct ObjectT {
    const char* name;
    object_t* deps[MAXOBJS];
    usize ndeps;
    objstate_t state;
    
    u64 base;
    Elf64_Dyn* dynbase;

    Elf64_Sym* dynsym;
    usize dynsymentsz;

    u64* got;
    void* pltrel_base;
    usize pltrel_sz;//
    u64 pltrel_type;//
    
    char* dynstr;
    u32* htab;
    u32* gnu_htab;

    void (**preinitarray)(void);
    usize preinitarraysz;

    void (*init)(void);
    void (**initarray)(void);
    usize initarraysz;

    void (*fini)(void);
    void (**finiarray)(void);
    usize finiarraysz;
};

typedef enum {
    LDSO_GETAUXVAL,
    LDSO_LDCLEANUP
} ldso_private_t;

HIDDEN ASMFUNC void* ldso_mmap(void* addr, u64 phys, u64 npgs, u64 flags);
HIDDEN ASMFUNC void* ldso_munmap(void* addr, u64 npgs, u64 flags);
HIDDEN ASMFUNC int ldso_mprotect(void* addr, u64 npgs, u64 flgs);
HIDDEN ASMFUNC int ldso_open(const char* path, int flags, u16 mode);
HIDDEN ASMFUNC int ldso_close(int fd);
HIDDEN ASMFUNC ssize ldso_read(int fd, void* buf, usize sz);
HIDDEN ASMFUNC s64 ldso_lseek(int fd, s64 off, int whence);
HIDDEN ASMFUNC NORETURN void ldso_exit(int code);
HIDDEN void ldso_print(const char* buf);
HIDDEN void ldso_print_hex(u64 val);
HIDDEN void* memset(void* dest, int c, usize n);
HIDDEN void* memcpy(void* dest, const void* src, usize count);
HIDDEN ASMFUNC int vmm_setflgs(u64 virt, u64 npgs, u64 flags);
HIDDEN usize strlen(const char* str);
HIDDEN s32 streq(const char* s1, const char* s2);

ASMFUNC void ldso_resolve();
HIDDEN u64 locate_extern(const char* name, usize* sz);
HIDDEN int apply_rel(Elf64_Rel* rel, object_t* obj);
HIDDEN int apply_rela(Elf64_Rela* rela, object_t* obj);
HIDDEN void apply_reltbl(Elf64_Rel* rels, usize reltbl_sz, object_t* obj);
HIDDEN void apply_relatbl(Elf64_Rela* relas, usize relatbl_sz, object_t* obj);

u64 getauxval(u64 type);
void __ldso_ldcleanup();

extern HIDDEN u64 _ldso_base;
extern HIDDEN Elf64_Auxv* ldso_auxv;
extern HIDDEN u64 __atmmaplow_vaddr;
extern HIDDEN u64 __atmmaphigh_vaddr;
extern HIDDEN u64 nloaded;
extern HIDDEN object_t objects[MAXOBJS];
extern HIDDEN object_t ldso_obj;