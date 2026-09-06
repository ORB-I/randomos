#include "ldso.h"
#include "sys/elf.h"
#include "sys/types.h"

HIDDEN object_t objects[MAXOBJS];
HIDDEN usize nloaded = 0;
HIDDEN u64 lodbase = 0;
HIDDEN object_t* exeobj = NULL;

// exe only
HIDDEN void run_preinits(object_t* obj) {
    if (obj->state == OBJ_LOADED) {
        if (obj->preinitarraysz > 0 && obj->preinitarray) {
            usize n = obj->preinitarraysz / sizeof(*obj->preinitarray);
            for (usize i = 0; i < n; i++) {
                if (obj->preinitarray[i]) obj->preinitarray[i]();
            }
        }
    }
    obj->state = OBJ_PREINITED;
}

HIDDEN void run_inits(object_t* obj) {
    if (!obj || obj->state == OBJ_INITED) return;

    for (usize i = 0; i < obj->ndeps; i++) {
        run_inits(obj->deps[i]);
    }

    if (obj->state == OBJ_PREINITED || obj->state == OBJ_LOADED) {
        if (obj->initarraysz > 0 && obj->initarray) {
            usize n = obj->initarraysz / sizeof(*obj->initarray);
            for (usize i = 0; i < n; i++) {
                if (obj->initarray[i]) obj->initarray[i]();
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

HIDDEN u64 _ldso_base;
HIDDEN Elf64_Auxv* ldso_auxv;
HIDDEN u64 __atmmaplow_vaddr = 0;
HIDDEN u64 __atmmaphigh_vaddr = 0;


u64 getauxval(u64 type) {
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

void __ldso_ldcleanup() {
    run_finis(exeobj);
}

HIDDEN object_t* load_library(const char* path, usize lodbase, usize* ldsz);
HIDDEN object_t* parse_object(u64 ldbase, u64 dynbase, const char* name) {
    Elf64_Dyn* dyn = (Elf64_Dyn*)dynbase;
    object_t* obj = &objects[nloaded++];
    obj->name = name;
    obj->dynbase = (Elf64_Dyn*)dynbase;
    obj->base = ldbase;
    obj->state = OBJ_LOADED;

    while (dyn->d_tag != DT_NULL) {
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
                break;
            }
            case DT_GNU_HASH: {
                obj->gnu_htab = (u32*)(ldbase + dyn->d_un.d_ptr);
                break;
            }
        }
        dyn++;
    }

    if (!obj->dynstr || !obj->dynsym || !obj->dynsymentsz || (!obj->htab && !obj->gnu_htab)) {
        ldso_exit(1);
    }

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
                const char* needed_name = obj->dynstr + dyn->d_un.d_val;
                if (!(lib = load_library(needed_name, lodbase, &ldsz))) {
                    ldso_exit(1);
                }

                obj->deps[obj->ndeps++] = lib;
                lodbase += (ldsz + 0xFFF) & ~0xFFFULL;
                break;
            }

            case DT_PLTGOT: obj->got = (u64*)(ldbase + dyn->d_un.d_ptr); break;

            case DT_JMPREL: obj->pltrel_base = (void*)(ldbase + dyn->d_un.d_ptr); break;
            case DT_PLTRELSZ: obj->pltrel_sz = dyn->d_un.d_val; break;
            case DT_PLTREL: obj->pltrel_type = dyn->d_un.d_val; break;
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
        dyn++;
    }

    if (!obj->got) {
        ldso_print("No global offset table in object ");
        ldso_print(name);
        ldso_print(". Will not be able to use external symbols.");
        return obj;
    }

    if (obj->pltrel_base) {
        if (obj->pltrel_type == DT_REL) {
            apply_reltbl((Elf64_Rel*)obj->pltrel_base, obj->pltrel_sz, obj);
        } else if (obj->pltrel_type == DT_RELA) {
            apply_relatbl((Elf64_Rela*)obj->pltrel_base, obj->pltrel_sz, obj);
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

    obj->got[1] = (u64)obj;
    obj->got[2] = (u64)ldso_resolve;
    return obj;
}

HIDDEN object_t* load_library(const char* path, usize lodbase, usize* ldsz) {
    ldso_print("loading library ");
    ldso_print(path);
    ldso_print(" at load base ");
    ldso_print_hex(lodbase);

    for (usize i = 0; i < nloaded; i++) {
        if (objects[i].name && streq(objects[i].name, path)) {
            if (ldsz) *ldsz = 0;
            return &objects[i];
        }
    }

    int fd = ldso_open(path, O_RDONLY, 0);
    if (fd < 0 && path[0] != '/') {
        char buf[256];
        const char* prefix = "/lib/";
        usize pl = strlen(prefix);
        usize pathl = strlen(path);
        if (pl + pathl < sizeof(buf)) {
            memcpy(buf, prefix, pl);
            memcpy(buf + pl, path, pathl + 1);
            fd = ldso_open(buf, O_RDONLY, 0);
        }
    }
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
    return parse_object(lodbase, dynbase, path);
}

HIDDEN void ldso_main(u64 ldso_base, u64 argc, char** argv, char** envp, Elf64_Auxv* auxv) {

    ldso_auxv = auxv;
    _ldso_base = ldso_base;

    u64 phdrs_addr = getauxval(AT_PHDR);
    u64 phnum = getauxval(AT_PHNUM);
    u64 entry = getauxval(AT_ENTRY);
    u64 stack = getauxval(AT_STACK);
    u64 stksz = getauxval(AT_STACKSZ);
    __atmmaphigh_vaddr = stack - stksz;

    Elf64_Phdr* phdrs = (Elf64_Phdr*)phdrs_addr;
    u64 phdr_vaddr = 0;
    usize dynidx = 0;
    int hasdyn = 0;

    for (usize i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dynidx = i;
            hasdyn = 1;
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

    Elf64_Phdr* iphdrs = (Elf64_Phdr*)getauxval(AT_IPHDRS);
    u64 iphnum = getauxval(AT_IPHNUM);

    u64 load_high = 0;
    for (usize i = 0; i < iphnum; i++) {
        if (iphdrs[i].p_type == PT_LOAD) {
            u64 seghigh = (iphdrs[i].p_vaddr + ldso_base) + iphdrs[i].p_memsz;
            if (seghigh > load_high) load_high = seghigh;
        }
    }
    lodbase = (load_high + 0xFFF) & ~0xFFFULL;

    u64 exebase = (u64)phdrs - phdr_vaddr;
    object_t* exeobj = parse_object(exebase, exebase + phdrs[dynidx].p_vaddr, "main");
    __atmmaplow_vaddr = lodbase;

    run_preinits(exeobj);
    run_inits(exeobj);

    int ret = ((int (*)(int argc, char** argv, char** envp))entry)(argc, argv, envp);

    run_finis(exeobj); // libc should clean up, but run finis just in case libc returned
    ldso_exit(ret);
}