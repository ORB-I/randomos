#include <sys/types.h>
#include <sys/elf.h>

#define HIDDEN __attribute__((visibility("hidden")))
#define ASMFUNC __attribute((naked))
#define NORETURN __attribute__((noreturn))

HIDDEN NORETURN void ldso_start(void* rsp);
HIDDEN NORETURN HIDDEN void ldso_main(u64 ldso_base, u64 argc, char** argv, char** envp, Elf64_Auxv* auxv);

ASMFUNC NORETURN void _start(void) {
    asm volatile(
        "mov %rsp, %rdi\n\t"
        "and $-16, %rsp\n\t"
        "jmp ldso_start"
    );
}

// elf needs to parse its own DT_RELA and DT_REL first
HIDDEN void ldso_start(void* rsp) {
    asm volatile(
        "mov %0, %%rdi\n\t"
        "mov $5, %%rsi\n\t"
        "mov $54, %%rax\n\t"
        "syscall\n\t"
        : "=r"("ldso\n")
        :: "rdi", "rax"
    );

    u64* p = rsp;
    u64 argc = *p++;
    char** argv = (char**)p;

    p += argc + 1;
    char** envp = (char**)p;
    while (*p++);

    Elf64_Auxv* auxv = (Elf64_Auxv*)p;
    Elf64_Auxv* actual_auxv = auxv;

    usize phdrs_addr = 0;
    usize phnum = 0;

    while (auxv->type != AT_NULL) {
        if (auxv->type == AT_IPHDRS) {
            phdrs_addr = auxv->val;
        } else if (auxv->type == AT_IPHNUM) {
            phnum = auxv->val;
        }
        auxv++;
    }

    if (!phdrs_addr || phnum == 0) {
        // like exit(1); but (hopefully) without needing the
        // function relocations lol
        asm volatile(
            "mov $1, %rdi\n\t"
            "mov $1, %rax\n\t"
            "syscall"
        );
    }

    Elf64_Phdr* phdrs = (Elf64_Phdr*)phdrs_addr;
    usize dyns_addr = 0;
    usize ldso_base = 0;
    for (usize i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_PHDR) {
            ldso_base = phdrs_addr - phdrs[i].p_vaddr;
        } else if (phdrs[i].p_type == PT_DYNAMIC) {
            dyns_addr = phdrs[i].p_vaddr;
        }
    }

    if (!ldso_base) {
        asm volatile(
            "mov $1, %rdi\n\t"
            "mov $1, %rax\n\t"
            "syscall"
        );
    }

    if (dyns_addr) {
        Elf64_Dyn* dyns = (Elf64_Dyn*)(ldso_base + dyns_addr);
        
        usize rela_addr = 0;
        usize rela_sz = 0;

        usize rel_addr = 0;
        usize rel_sz = 0;

        while (dyns->d_tag != DT_NULL) {
            switch (dyns->d_tag) {
                case DT_RELA: rela_addr = dyns->d_un.d_ptr; break;
                case DT_RELASZ: rela_sz = dyns->d_un.d_val; break;
                case DT_REL: rel_addr = dyns->d_un.d_ptr; break;
                case DT_RELSZ: rel_sz = dyns->d_un.d_val; break;
            }
            dyns++;
        }

        if (rela_addr && rela_sz != 0) {
            Elf64_Rela* relas = (Elf64_Rela*)(ldso_base + rela_addr);
            usize nrelas = rela_sz / sizeof(Elf64_Rela);
            for (usize i = 0; i < nrelas; i++) {
                Elf64_Rela* rela = &relas[i];
                if (ELF64_R_TYPE(rela->r_info) == R_X86_64_RELATIVE) {
                    *((u64*)(ldso_base + rela->r_offset)) = ldso_base + rela->r_addend;
                }
            }
        }

        if (rel_addr && rel_sz != 0) {
            Elf64_Rel* rels = (Elf64_Rel*)(ldso_base + rel_addr);
            usize nrels = rel_sz / sizeof(Elf64_Rel);
            for (usize i = 0; i < nrels; i++) {
                Elf64_Rel* rel = &rels[i];
                if (ELF64_R_TYPE(rel->r_info) == R_X86_64_RELATIVE) {
                    u64 tgt = ldso_base + rel->r_offset;
                    u64 addend = *(u64*)tgt;
                    *((u64*)tgt) = ldso_base + addend;
                }
            }
        }
    }

    ldso_main(ldso_base, argc, argv, envp, actual_auxv);
}

HIDDEN ASMFUNC void* ldso_mmap(void* addr, u64 phys, u64 npgs, u64 flags) {
    asm volatile(
        "mov $36, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC void* ldso_munmap(void* addr, u64 npgs, u64 flags) {
    asm volatile(
        "mov $37, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC int ldso_mprotect(void* addr, u64 npgs, u64 flgs) {
    asm volatile(
        "mov $63, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

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

HIDDEN ASMFUNC int ldso_open(const char* path, int flags, u16 mode) {
    asm volatile(
        "mov $4, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC int ldso_close(int fd) {
    asm volatile(
        "mov $5, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC ssize ldso_read(int fd, void* buf, usize sz) {
    asm volatile(
        "mov $2, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC s64 ldso_lseek(int fd, s64 off, int whence) {
    asm volatile(
        "mov $9, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN ASMFUNC NORETURN void ldso_exit(int code) {
    asm volatile(
        "mov $1, %rax\n\t"
        "syscall\n\t"
    );
}

HIDDEN void ldso_print(const char* buf) {
    while (*buf != '\0') {
        asm volatile(
            "mov %0, %%rdi\n\t"
            "mov $1, %%rsi\n\t"
            "mov $54, %%rax\n\t"
            "syscall\n\t"
            : "=r"(buf)
            :: "rdi", "rax"
        );
    }
}

HIDDEN static u64 _ldso_base;
HIDDEN static Elf64_Auxv* ldso_auxv;
HIDDEN static u64 __atmmaplow_vaddr = 0;
HIDDEN static u64 __atmmaphigh_vaddr = 0;

HIDDEN u64 __ldso_getauxval(u64 type) {
    if (type == AT_MMAPLOW) {
        return __atmmaplow_vaddr;
    } else if (type == AT_MMAPHIGH) {
        return __atmmaphigh_vaddr;
    } else {
        Elf64_Auxv* auxv = (Elf64_Auxv*)ldso_auxv;
        while (auxv->type != AT_NULL) {
            if (auxv->type == type) {
                return auxv->val;
            }
            auxv++;
        }
        return 0;
    }
}

HIDDEN void* memset(void* dest, int c, usize n) {
    void* orig = dest;
    u8 val = (u8)c;
    asm volatile(
        "cld\n\t"
        "rep stosb"
        : "+D"(dest), "+c"(n)
        : "a"(val)
        : "memory"
    );
    return orig;
}

HIDDEN void* memcpy(void* dest, const void* src, usize count) {
    void* orig = dest;
    asm volatile(
        "cld\n\t"
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(count)
        :: "memory"
    );
    return orig;
}

typedef enum {
    OBJ_LOADED,
    OBJ_PREINITED,
    OBJ_INITED,
    OBJ_FINIED
} objstate_t;

#define MAXOBJS 128
typedef struct ObjectT object_t;
struct ObjectT {
    object_t* deps[MAXOBJS];
    usize ndeps;
    objstate_t state;
    
    u64 base;
    Elf64_Dyn* dynbase;

    Elf64_Sym* dynsym;
    usize dynsymentsz;
    
    char* dynstr;
    u32* htab;

    void (**preinitarray)(void);
    usize preinitarraysz;

    void (*init)(void);
    void (**initarray)(void);
    usize initarraysz;

    void (*fini)(void);
    void (**finiarray)(void);
    usize finiarraysz;
};
HIDDEN static object_t objects[MAXOBJS];
HIDDEN static usize nloaded = 0;
HIDDEN static u64 lodbase = 0;

typedef u64 page_table_t;
#define PML4_IDX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((addr) >> 12) & 0x1FF)
#define HHDM_START 0xFFFF800000000000

HIDDEN ASMFUNC int vmm_setflgs(u64 virt, u64 npgs, u64 flags) {
    asm volatile(
        "mov $63, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
}

HIDDEN usize strlen(const char* str) {
    const char* orig = str;
    asm volatile(
        "cld\n\t"
        "repne scasb\n\t"
        : "+D"(str)
        : "a"(0), "c"(-1)
        : "memory", "cc"
    );
    return (str - orig) - 1;
}

HIDDEN s32 streq(const char* s1, const char* s2) {
    usize s1sz = strlen(s1);
    usize s2sz = strlen(s2);

    if (s1sz != s2sz) return 0;

    for (usize i = 0; i < s1sz; i++) {
        if (s1[i] != s2[i]) return 0;
    }

    return 1;
}

HIDDEN u64 elf_hash(const char* name) {
    u64 h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if (g) { h ^= g >> 24; }
        h &= ~g;
    }
    return h;
}

// this should switch to also supporting DT_GNU_HASH sometime
HIDDEN u64 locate_hashsym(object_t* obj, const char* name, usize* sz) {
    u32 h = elf_hash(name);

    u32 nbuckets = *obj->htab;
    u32* buckets = obj->htab + 2;
    u32* chains = buckets + nbuckets;

    u32 i = buckets[h % nbuckets];
    while (i != STN_UNDEF) {
        Elf64_Sym* sym = &obj->dynsym[i];
        if (streq(obj->dynstr + sym->st_name, name) == 0) {
            if (sz) *sz = sym->st_size;
            return obj->base + sym->st_value;
        }
        i = chains[i];
    }
    return STN_UNDEF;
}

HIDDEN u64 locate_extern(const char* name, usize* sz) {
    if (streq(name, "__ldso_getauxval")) {
        return (u64)__ldso_getauxval;
    }

    for (usize l = 0; l < nloaded; l++) {
        object_t* obj = &objects[l];
        u64 i = locate_hashsym(obj, name, sz);
        if (i != STN_UNDEF) {
            return i;
        }
    }
    return 0;
}

HIDDEN int apply_rel(Elf64_Rel* rel, object_t* obj) {
    u64 tgt = obj->base + rel->r_offset;
    u64 addend = *(u64*)tgt;
    u64 v2r = 0;

    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_RELATIVE: {
            v2r = obj->base + addend;
            break;
        }
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_GLOB_DAT: {
            Elf64_Sym* sym = &obj->dynsym[ELF64_R_SYM(rel->r_info)];
            u64 symaddr = 0;

            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = obj->base + sym->st_value;
            } else {
                symaddr = locate_extern(obj->dynstr + sym->st_name, NULL);
                if (!symaddr) {
                    ldso_exit(1);
                }
            }
            v2r = symaddr + addend;
            break;
        }
        case R_X86_64_COPY: {
            Elf64_Sym* sym = &obj->dynsym[ELF64_R_SYM(rel->r_info)];
            u64 symaddr;
            usize sz = 0;
            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = obj->base + sym->st_value;
                sz = sym->st_size;
            } else {
                symaddr = locate_extern(obj->dynstr + sym->st_name, &sz);
                if (!symaddr) {
                    ldso_exit(1);
                }
            }

            memcpy((void*)tgt, (void*)symaddr, sz);
            return 0;
        }
        default: return 0;
    }

    *((u64*)tgt) = v2r;
    return 0;
}

HIDDEN int apply_rela(Elf64_Rela* rela, object_t* obj) {
    u64 tgt = obj->base + rela->r_offset;
    u64 v2r = 0;

    switch (ELF64_R_TYPE(rela->r_info)) {
        case R_X86_64_RELATIVE: {
            v2r = obj->base + rela->r_addend;
            break;
        }
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_GLOB_DAT: {
            Elf64_Sym* sym = &obj->dynsym[ELF64_R_SYM(rela->r_info)];
            u64 symaddr = 0;

            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = obj->base + sym->st_value;
            } else {
                symaddr = locate_extern(obj->dynstr + sym->st_name, NULL);
                if (!symaddr) {
                    ldso_exit(1);
                }
            }
            v2r = symaddr + rela->r_addend;
            break;
        }
        case R_X86_64_COPY: {
            Elf64_Sym* sym = &obj->dynsym[ELF64_R_SYM(rela->r_info)];
            u64 symaddr;
            usize sz = 0;
            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = obj->base + sym->st_value;
                sz = sym->st_size;
            } else {
                symaddr = locate_extern(obj->dynstr + sym->st_name, &sz);
                if (!symaddr) {
                    ldso_exit(1);
                }
            }

            memcpy((void*)tgt, (void*)symaddr, sz);
        }
        default: return 0;
    }

    *((u64*)tgt) = v2r;
    return 0;
}

HIDDEN void apply_reltbl(Elf64_Rel* rels, usize reltbl_sz, object_t* obj) {
    usize nrels = reltbl_sz / sizeof(Elf64_Rel);
    for (usize i = 0; i < nrels; i++) {
        apply_rel(&rels[i], obj);
    }
}

HIDDEN void apply_relatbl(Elf64_Rela* relas, usize relatbl_sz, object_t* obj) {
    usize nrelas = relatbl_sz / sizeof(Elf64_Rela);
    for (usize i = 0; i < nrelas; i++) {
        apply_rela(&relas[i], obj);
    }
}

HIDDEN object_t* load_library(const char* path, usize lodbase, usize* ldsz);
HIDDEN object_t* parse_object(u64 ldbase, u64 dynbase) {
    Elf64_Dyn* dyn = (Elf64_Dyn*)dynbase;
    object_t* obj = &objects[nloaded++];
    obj->dynbase = (Elf64_Dyn*)dynbase;
    obj->base = ldbase;
    obj->state = OBJ_LOADED;

    while (dyn->d_tag != DT_NULL) {
        if (!obj->dynstr || !obj->dynsym || !obj->dynsymentsz || !obj->htab) {
            switch (dyn->d_tag) {
                case DT_SYMTAB: {
                    obj->dynsym = (Elf64_Sym*)(ldbase + dyn->d_un.d_ptr);
                    break;
                }
                case DT_SYMENT: {
                    obj->dynsymentsz = dyn->d_un.d_val;
                    break;
                }
                case DT_STRTAB: {
                    obj->dynstr = (char*)(ldbase + dyn->d_un.d_ptr);
                    break;
                }
                case DT_HASH: {
                    obj->htab = (u32*)(ldbase + dyn->d_un.d_ptr);
                }
            }
        } else {
            break;
        }
    }

    if (!obj->dynstr || !obj->dynsym || !obj->dynsymentsz || !obj->htab) {
        ldso_exit(1);
    }

    void* pltrel_base = 0;
    usize pltrel_sz = 0;
    u64 pltrel_type = 0;

    void* rela = 0;
    usize relasz = 0;

    void* rel = 0;
    usize relsz = 0;

    dyn = (Elf64_Dyn*)dynbase;
    while (dyn->d_tag != DT_NULL) {
        switch (dyn->d_tag) {
            case DT_NEEDED: {
                if (nloaded + 1 > MAXOBJS) {
                    break;
                }

                usize ldsz = 0;
                object_t* lib = NULL;
                if (!(lib = load_library(obj->dynstr + (ldbase + dyn->d_un.d_ptr), lodbase, &ldsz))) {
                    ldso_exit(1);
                }

                obj->deps[obj->ndeps++] = lib;
                lodbase += (ldsz + 0xFFF) & ~0xFFFULL;
                break;
            }

            case DT_JMPREL: pltrel_base = (void*)(ldbase + dyn->d_un.d_ptr); break;
            case DT_PLTRELSZ: pltrel_sz = dyn->d_un.d_val; break;
            case DT_PLTREL: pltrel_type = dyn->d_un.d_val; break;
            case DT_RELA: rela = (Elf64_Rela*)(ldbase + dyn->d_un.d_ptr); break;
            case DT_RELASZ: relasz = dyn->d_un.d_val; break;
            case DT_REL: rel = (Elf64_Rel*)(ldbase + dyn->d_un.d_ptr); break;
            case DT_RELSZ: relsz = dyn->d_un.d_val; break;

            // we just ignore these for now, but will need to soon support them
            // so that c++ is properly supported
            case DT_INIT: obj->init = (void(*)(void))(ldbase + dyn->d_un.d_ptr); break;
            case DT_FINI: obj->fini = (void(*)(void))(ldbase + dyn->d_un.d_ptr); break;
            case DT_INITARRAY: obj->initarray = (void(**)(void))(ldbase + dyn->d_un.d_ptr); break;
            case DT_FINIARRAY: obj->finiarray = (void(**)(void))(ldbase + dyn->d_un.d_ptr); break;
            case DT_INITARRAYSZ: obj->initarraysz = dyn->d_un.d_val; break;
            case DT_FINIARRAYSZ: obj->finiarraysz = dyn->d_un.d_val; break;
            case DT_PREINITARRAY: obj->preinitarray = (void(**)(void))(ldbase + dyn->d_un.d_ptr); break;
            case DT_PREINITARRAYSZ: obj->preinitarraysz = dyn->d_un.d_val; break;
        }
    }

    if (pltrel_base) {
        if (pltrel_type == DT_REL) {
            apply_reltbl((Elf64_Rel*)pltrel_base, pltrel_sz, obj);
        } else if (pltrel_type == DT_RELA) {
            apply_relatbl((Elf64_Rela*)pltrel_base, pltrel_sz, obj);
        } else {
            ldso_exit(1);
        }
    }

    if (rel) {
        apply_reltbl((Elf64_Rel*)rel, relsz, obj);
    }

    if (rela) {
        apply_relatbl((Elf64_Rela*)rela, relasz, obj);
    }

    return 0;
}

HIDDEN object_t* load_library(const char* path, usize lodbase, usize* ldsz) {
    int fd = ldso_open(path, O_RDONLY, 0);
    if (fd < 0) {
        ldso_exit(1);
    }

    Elf64_Ehdr ehdr;
    ssize nread = ldso_read(fd, &ehdr, sizeof(ehdr));
    if (nread < 0 || (usize)nread < sizeof(ehdr)) {
        ldso_close(fd);
        ldso_exit(1);
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            ldso_close(fd);
            ldso_exit(1);
    }

    if ((ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            ldso_close(fd);
            ldso_exit(1);
    }

    if (ldso_lseek(fd, ehdr.e_phoff, SEEK_SET) < 0) {
        ldso_close(fd);
        ldso_exit(1);
    }

    Elf64_Phdr phdrs[ehdr.e_phnum];

    usize ldhigh = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = ldso_read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != sizeof(Elf64_Phdr)) {
            ldso_close(fd);
            ldso_exit(1);
        }

        u64 seg_vaddr = phdrs[i].p_vaddr + lodbase;
        u64 seghigh = seg_vaddr + phdrs[i].p_memsz;
        if (seghigh >= __atmmaphigh_vaddr) {
            ldso_close(fd);
            ldso_exit(1);
        }

        if (seghigh > ldhigh) ldhigh = seghigh;
    }

    u64 dynbase = 0;
    for (usize i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (phdrs[i].p_memsz == 0) continue;
            u64 segvaddr = lodbase + phdrs[i].p_vaddr;
            u64 stpage = segvaddr & ~0xFFFULL;
            u64 endpage = (segvaddr + phdrs[i].p_memsz + 0xFFFULL) & ~0xFFFULL;
            usize npgs = (usize)((endpage - stpage) / 4096);

            void* mapped = ldso_mmap((void*)stpage, 0, npgs, MAP_ANYPHYS | MAP_CONT | PAGE_WRITE);
            if (!mapped) ldso_exit(1);

            void* addr = (void*)segvaddr;

            if (ldso_lseek(fd, phdrs[i].p_offset, SEEK_SET) < 0) {
                ldso_exit(1);
            }

            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset((void*)(segvaddr + phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
            }

            ssize nread = ldso_read(fd, addr, phdrs[i].p_filesz);
            if (nread < 0 || (usize)nread < phdrs[i].p_filesz) {
                ldso_exit(1);
            }

            u64 flgs = 0; // kern auto-applied PAGE_USER
            if (phdrs[i].p_flags & PF_W) {
                flgs |= PAGE_WRITE;
            }

            if (vmm_setflgs(stpage, npgs, flgs) < 0) {
                ldso_exit(1);
            }
        } else if (phdrs[i].p_type == PT_DYNAMIC) {
            dynbase = lodbase + phdrs[i].p_vaddr;
        }
    }

    if (ldsz) *ldsz = ldhigh - lodbase;
    return parse_object(lodbase, dynbase);
}

// exe only
HIDDEN void run_preinits(object_t* obj) {
    if (obj->state == OBJ_LOADED) {
        if (obj->preinitarraysz > 0) {
            for (usize i = 0; i < obj->preinitarraysz; i++) {
                obj->preinitarray[i]();
            }
        }
    }
    obj->state = OBJ_PREINITED;
}

HIDDEN void run_inits(object_t* obj) {
    for (usize i = 0; i < obj->ndeps; i++) {
        run_inits(obj->deps[i]);
    }

    if (obj->state == OBJ_PREINITED) {
        if (obj->initarraysz > 0) {
            for (usize i = 0; i < obj->initarraysz / sizeof(*obj->initarray); i++) {
                obj->initarray[i]();
            }
        }

        if (obj->init) {
            obj->init();
        }
    }
    obj->state = OBJ_INITED;
}

HIDDEN void run_finis(object_t* obj) {
    if (obj->state == OBJ_INITED) {
        if (obj->finiarray) {
            usize n = obj->finiarraysz / sizeof(*obj->finiarray);

            for (usize i = 0; i < n; i++) {
                obj->finiarray[i]();
            }

        }

        if (obj->fini) obj->fini();
        obj->state = OBJ_FINIED;
    }

    for (usize i = 0; i < obj->ndeps; i++) {
        run_finis(obj->deps[i]);
    }
}

HIDDEN void ldso_main(u64 ldso_base, u64 argc, char** argv, char** envp, Elf64_Auxv* auxv) {
    ldso_auxv = auxv;
    _ldso_base = ldso_base;

    u64 phdrs_addr = __ldso_getauxval(AT_PHDR);
    u64 phnum = __ldso_getauxval(AT_PHNUM);
    u64 entry = __ldso_getauxval(AT_ENTRY);
    u64 stack = __ldso_getauxval(AT_STACK);
    u64 stksz = __ldso_getauxval(AT_STACKSZ);
    __atmmaphigh_vaddr = stack - stksz;

    Elf64_Phdr* phdrs = (Elf64_Phdr*)phdrs_addr;
    u64 phdr_vaddr = 0;
    usize dynidx = 0;
    int hasdyn = 0;

    for (usize i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dynidx = i;
            hasdyn = 1;
            break;
        } else if (phdrs[i].p_type == PT_PHDR) {
            phdr_vaddr = phdrs[i].p_vaddr;
        }
    }

    if (!hasdyn) {
        int ret = ((int (*)(int argc, char** argv, char** envp))entry)(argc, argv, envp);
        ldso_exit(ret);
    }

    // we need to find our highest load address
    // given that the kernel
    // - loads segments according to p_memsz
    // - loads the ld.so AFTER the executable in memory

    Elf64_Phdr* iphdrs = (Elf64_Phdr*)__ldso_getauxval(AT_IPHDRS);
    u64 iphnum = __ldso_getauxval(AT_IPHNUM);

    u64 load_high = 0;
    for (usize i = 0; i < iphnum; i++) {
        if (iphdrs[i].p_type == PT_LOAD) {
            u64 seghigh = (phdrs[i].p_vaddr + ldso_base) + phdrs[i].p_memsz;
            if (seghigh > load_high) load_high = seghigh;
        }
    }
    lodbase = (load_high + 0xFFF) & ~0xFFFULL;
    
    // now we can get to the fun part
    // - grab symtab and strtab for symbol resolution
    // - load libraries (perform all this for each)
    // - parse relocations
    // - run preinits
    // - run inits
    // - jump to main
    // - run finis
    // - ldso_exit

    u64 exebase = (u64)phdrs - phdr_vaddr;
    object_t* exeobj = parse_object(exebase, exebase + phdrs[dynidx].p_vaddr);
    __atmmaplow_vaddr = lodbase;

    run_preinits(exeobj);
    run_inits(exeobj);
    
    int ret = ((int (*)(int argc, char** argv, char** envp))entry)(argc, argv, envp);

    run_finis(exeobj);
    ldso_exit(ret);
}