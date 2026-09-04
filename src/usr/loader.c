#include <core/elf.h>
#include <drivers/storage/fs.h>
#include <lib/string.h>
#include <core/mem/vmm.h>
#include <core/liballoc.h>
#include <core/kprint.h>
#include <drivers/display/term.h>
#include <lib/loader.h>
#include <lib/syscall.h>
#include <core/kprint.h>
#include <core/asmh.h>
#include <core/errno.h>

// we should eventually (soon) move a 
// bunch of this into a seperate ld.so

#define USTACK    (16 * 4096)
#define USTACKPGS 16
#define ARGMAX    16

extern u64 ram_max;

typedef struct {
    int code;
    void* ptr;
    usize npgs;
} segment_ld_t;
#define SEGLD_ERR(ERRNO) ((segment_ld_t){ERRNO,NULL,0})

void clrksegs(segment_ld_t* segs, usize nsegs) {
    for (usize i = 0; i < nsegs; i++) {
        kprint("unloading segment %lu (%p) from kernel\n", i, segs[i].ptr);
        vmm_unmap_pages(vmm_cpml4v(), (u64)segs[i].ptr, segs[i].npgs, UNMAP_KEEPPHYS);
    }
}

segment_ld_t load_segment(Elf64_Phdr* phdr, int fd, page_table_t* nasp, u64 load_base) {
    if (phdr->p_memsz == 0) return SEGLD_ERR(EOK);
    u64 seg_vaddr = load_base + phdr->p_vaddr;

    u64 start_page = seg_vaddr & ~0xFFFULL;
    u64 end_page = (seg_vaddr + phdr->p_memsz + 0xFFFULL) & ~0xFFFULL;
    usize npgs = (usize)((end_page - start_page) / 4096);

    void* mapped = vmm_map_pages(vmm_cpml4v(), start_page, 0, npgs, MAP_ANYPHYS | MAP_CONT | PAGE_WRITE);
    if (!mapped) return SEGLD_ERR(-ENOMEM);

    void* addr = (void*)seg_vaddr;

    int ret = 0;
    if ((ret = lseek(fd, phdr->p_offset, SEEK_SET)) < 0) {
        kprint("Loader: failed to seek phdr offset\n");
        return SEGLD_ERR(ret);
    }

    if (phdr->p_memsz > phdr->p_filesz) {
        memset((void*)(seg_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
    }

    ssize nread = read(fd, addr, phdr->p_filesz);
    if (nread < 0 || (usize)nread < phdr->p_filesz) {
        kprint("Loader: failed to read program data\n");
        return SEGLD_ERR(nread);
    }

    u64 flgs = PAGE_USER;
    if (phdr->p_flags & PF_W) {
        flgs |= PAGE_WRITE;
    }

    u64 paddr = vmm_get_phys(vmm_cpml4v(), start_page);
    if (!vmm_map_pages(nasp, start_page, paddr, npgs, MAP_CONT | flgs)) {
        return SEGLD_ERR(ENOMEM);
    }

    return (segment_ld_t){0, (void*)start_page, npgs};
}

#define MAX_LIBRARIES 128
typedef struct {
    u64 base;
    u64 dynbase;
    Elf64_Sym* dynsym;
    usize dynsymentsz;
    char* dynstr;
    u32* htab;
    segment_ld_t* segs;
    usize nldsegs;
} dyninfo_t;
dyninfo_t loaded[MAX_LIBRARIES];
usize nloaded = 0;
u64 lodbase = 0;

void clrsegs_err() {
    for (usize i = 0; i < nloaded; i++) {
        for (usize i = 0; i < loaded[i].nldsegs; i++) {
            vmm_unmap_pages(vmm_cpml4v(), (u64)loaded[i].segs[i].ptr, loaded[i].segs[i].npgs, 0);
        }
    }
}

#define MSR_KERNEL_GS_BASE 0xC0000102
extern __align(16) u8 kern_stack[65536];
static u64 gsblk[2];
void reset_kgsb() {
    gsblk[0] = 0x00007FFFFFFFF000;
    gsblk[1] = (u64)kern_stack + sizeof(kern_stack);
    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
}

u64 elf_hash(const char* name) {
    u64 h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if (g) { h ^= g >> 24; }
        h &= ~g;
    }
    return h;
}

u64 locate_hashsym(dyninfo_t* info, const char* name, usize* sz) {
    u32 h = elf_hash(name);

    u32 nbuckets = *info->htab;
    u32* buckets = info->htab + 2;
    u32* chains = buckets + nbuckets;

    u32 i = buckets[h % nbuckets];
    while (i != STN_UNDEF) {
        Elf64_Sym* sym = &info->dynsym[i];
        if (strcmp(info->dynstr + sym->st_name, name) == 0) {
            if (sz) *sz = sym->st_size;
            return info->base + sym->st_value;
        }
        i = chains[i];
    }
    return STN_UNDEF;
}

u64 locate_extern(const char* name, usize* sz) {
    for (usize l = 0; l < nloaded; l++) {
        dyninfo_t* info = &loaded[l];
        kprint("searching library %lu for symbol %s\n", l, name);
        u64 i = locate_hashsym(info, name, sz);
        if (i != STN_UNDEF) {
            return i;
        }
    }
    return 0;
}

int apply_rel(Elf64_Rel* rel, dyninfo_t* info) {
    u64 tgt_vaddr = info->base + rel->r_offset;
    u64 addend = *(u64*)tgt_vaddr;
    u64 v2r = 0;

    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_RELATIVE: {
            v2r = info->base + addend;
            break;
        }
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_GLOB_DAT: {
            Elf64_Sym* sym = &info->dynsym[ELF64_R_SYM(rel->r_info)];
            u64 symaddr = 0;

            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = info->base + sym->st_value;
            } else {
                symaddr = locate_extern(info->dynstr + sym->st_name, NULL);
                if (!symaddr) {
                    kprint("Loader: symbol resolution failed for %s\n", info->dynstr + sym->st_name);
                    return -ENOEXIST;
                }
            }
            v2r = symaddr + addend;
            break;
        }
        case R_X86_64_COPY: {
            Elf64_Sym* sym = &info->dynsym[ELF64_R_SYM(rel->r_info)];
            u64 symaddr;
            usize sz = 0;
            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = info->base + sym->st_value;
                sz = sym->st_size;
            } else {
                symaddr = locate_extern(info->dynstr + sym->st_name, &sz);
                if (!symaddr) {
                    kprint("Loader: symbol resolution failed for %s\n", info->dynstr + sym->st_name);
                    return -ENOEXIST;
                }
            }

            memcpy((void*)tgt_vaddr, (void*)symaddr, sz);
            return 0;
        }
        default: return 0;
    }

    *((u64*)tgt_vaddr) = v2r;
    return 0;
}

int apply_rela(Elf64_Rela* rela, dyninfo_t* info) {
    u64 tgt_vaddr = info->base + rela->r_offset;
    u64 v2r = 0;

    switch (ELF64_R_TYPE(rela->r_info)) {
        case R_X86_64_RELATIVE: {
            v2r = info->base + rela->r_addend;
            break;
        }
        case R_X86_64_JUMP_SLOT:
        case R_X86_64_GLOB_DAT: {
            Elf64_Sym* sym = &info->dynsym[ELF64_R_SYM(rela->r_info)];
            u64 symaddr = 0;

            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = info->base + sym->st_value;
            } else {
                symaddr = locate_extern(info->dynstr + sym->st_name, NULL);
                if (!symaddr) {
                    kprint("Loader: symbol resolution failed for %s\n", info->dynstr + sym->st_name);
                    return -ENOEXIST;
                }
            }
            v2r = symaddr + rela->r_addend;
            break;
        }
        case R_X86_64_COPY: {
            Elf64_Sym* sym = &info->dynsym[ELF64_R_SYM(rela->r_info)];
            u64 symaddr;
            usize sz = 0;
            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = info->base + sym->st_value;
                sz = sym->st_size;
            } else {
                symaddr = locate_extern(info->dynstr + sym->st_name, &sz);
                if (!symaddr) {
                    kprint("Loader: symbol resolution failed for %s\n", info->dynstr + sym->st_name);
                    return -ENOEXIST;
                }
            }

            memcpy((void*)tgt_vaddr, (void*)symaddr, sz);
        }
        default: return 0;
    }

    *((u64*)tgt_vaddr) = v2r;
    return 0;
}

int apply_reltbl(Elf64_Rel* rels, usize reltbl_sz, dyninfo_t* info) {
    int ret = 0;
    usize nrelas = reltbl_sz / sizeof(Elf64_Rel);
    for (usize i = 0; i < nrelas; i++) {
        if ((ret = apply_rel(&rels[i], info))) {
            kprint("Failed to apply relocations while loading program\n");
            return ret;
        }
    }
    return 0;
}

int apply_relatbl(Elf64_Rela* relas, usize relatbl_sz, dyninfo_t* info) {
    int ret = 0;
    usize nrelas = relatbl_sz / sizeof(Elf64_Rela);
    for (usize i = 0; i < nrelas; i++) {
        if ((ret = apply_rela(&relas[i], info))) {
            kprint("Failed to apply relocations while loading program\n");
            return ret;
        }
    }
    return 0;
}

int readoff(int fd, void* buf, usize sz, off_t off) {
    int ret = 0;
    if (lseek(fd, off, SEEK_SET) < 0) {
        return ret;
    }

    ssize nread = read(fd, buf, sz);
    if (nread < 0 || (usize)nread < sz) {
        return nread;
    }

    return 0;
}

struct dynproc_info {
    void* pltrel_base;
    u64 pltrel_sz;
    u64 pltrel_type; // DT_REL or DT_RELA

    Elf64_Rel* dynrel_base;
    u64 dynrel_sz;

    Elf64_Rela* dynrela_base;
    u64 dynrela_sz;

    // these are not used yet, but will be soon...
    void (**preinitarray)(void);   // ignored for sofile
    u64 preinitarraysz; // ignored for sofile

    void (*init)(void);
    void (**initarray)(void);
    u64 initarraysz;

    void (*fini)(void);
    void (**finiarray)(void);
    u64 finiarraysz;
};

int load_library(const char* path, u64 base, page_table_t* nasp, usize* ldsz);
int processdyn(u64 exebase, u64 dynbase, page_table_t* nasp) {
    Elf64_Dyn* dyn = (Elf64_Dyn*)dynbase;
    dyninfo_t* dynent = &loaded[nloaded++];
    dynent->dynbase = dynbase;
    kprint("processing dynamic object %zu\n", nloaded - 1);
    kprint("processing dynamic tags\n");

    dynent->base = exebase;
    while (dyn->d_tag != DT_NULL) {
        if (!dynent->dynstr || !dynent->dynsym || !dynent->dynsymentsz || !dynent->htab) {
            switch (dyn->d_tag) {
                case DT_SYMTAB: {
                    dynent->dynsym = (Elf64_Sym*)(exebase + dyn->d_un.d_ptr);
                    break;
                }
                case DT_SYMENT: {
                    dynent->dynsymentsz = dyn->d_un.d_val;
                    break;
                }
                case DT_STRTAB: {
                    dynent->dynstr = (char*)(exebase + dyn->d_un.d_ptr);
                    break;
                }
                case DT_HASH: {
                    dynent->htab = (u32*)(exebase + dyn->d_un.d_ptr);
                }
            }
        } else {
            break;
        }
        dyn++;
    }

    // first 3 are required by ELF spec
    // the hash table is a requirement we have
    if (!dynent->dynstr || !dynent->dynsym || !dynent->dynsymentsz || !dynent->htab) {
        return -EBADEXE;
    }

    dyn = (Elf64_Dyn*)dynbase;
    struct dynproc_info info = {0};
    while (dyn->d_tag != DT_NULL) {
        switch (dyn->d_tag) {
            case DT_NEEDED: {
                if (nloaded + 1 > MAX_LIBRARIES) {
                    break;
                }
                kprint("loading libraries\n");
                usize ldsz = 0;
                int res = load_library(dynent->dynstr + (exebase + dyn->d_un.d_ptr), lodbase, nasp, &ldsz);
                if (res < 0) return res;
                lodbase += (ldsz + 0xFFF) & ~0xFFFULL;
                kprint("loaded library\n");
                break;
            }

            case DT_JMPREL: info.pltrel_base = (void*)(exebase + dyn->d_un.d_ptr); break;
            case DT_PLTRELSZ: info.pltrel_sz = dyn->d_un.d_val; break;
            case DT_PLTREL: info.pltrel_type = dyn->d_un.d_val; break;
            case DT_RELA: info.dynrela_base = (Elf64_Rela*)(exebase + dyn->d_un.d_ptr); break;
            case DT_RELASZ: info.dynrela_sz = dyn->d_un.d_val; break;
            case DT_REL: info.dynrel_base = (Elf64_Rel*)(exebase + dyn->d_un.d_ptr); break;
            case DT_RELSZ: info.dynrel_sz = dyn->d_un.d_val; break;

            // we just ignore these for now, but will need to soon support them
            // so that c++ is properly supported
            case DT_INIT: info.init = (void(*)(void))(exebase + dyn->d_un.d_ptr); break;
            case DT_FINI: info.fini = (void(*)(void))(exebase + dyn->d_un.d_ptr); break;
            case DT_INITARRAY: info.initarray = (void(**)(void))(exebase + dyn->d_un.d_ptr); break;
            case DT_FINIARRAY: info.finiarray = (void(**)(void))(exebase + dyn->d_un.d_ptr); break;
            case DT_INITARRAYSZ: info.initarraysz = dyn->d_un.d_val; break;
            case DT_FINIARRAYSZ: info.finiarraysz = dyn->d_un.d_val; break;
            case DT_PREINITARRAY: info.preinitarray = (void(**)(void))(exebase + dyn->d_un.d_ptr); break;
            case DT_PREINITARRAYSZ: info.preinitarraysz = dyn->d_un.d_val; break;
        }
        dyn++;
    }

    kprint("processed dynamic tags\n");
    kprint("applying relocations\n");

    int ret = 0;
    if (info.pltrel_base) {
        if (info.pltrel_type == DT_REL) {
            kprint("applying PLT REL relocations\n");
            if ((ret = apply_reltbl((Elf64_Rel*)info.pltrel_base, info.pltrel_sz, dynent)) < 0) {
                return ret;
            }
            kprint("applied PLT REL relocations\n");
        } else if (info.pltrel_type == DT_RELA) {
            kprint("applying PLT RELA relocations\n");
            if ((ret = apply_relatbl((Elf64_Rela*)info.pltrel_base, info.pltrel_sz, dynent)) < 0) {
                return ret;
            }
            kprint("applied PLT RELA relocations\n");
        } else {
            kprint("unknown PLT relocation tpe %lu\n", info.pltrel_type);
            return -EBADEXE;
        }
    }

    if (info.dynrel_base) {
        if ((ret = apply_reltbl(info.dynrel_base, info.dynrel_sz, dynent)) < 0) {
            return ret;
        }
    }

    if (info.dynrela_base) {
        if ((ret = apply_relatbl(info.dynrela_base, info.dynrela_sz, dynent)) < 0) {
            return ret;
        }
    }

    kprint("processed relocations\n");
    kprint("processed dynamic object\n");
    return 0;
}

int load_library(const char* path, u64 base, page_table_t* nasp, usize* ldsz) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }

    kprint("Loading library %s at base %lu\n", path, base);

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread < 0 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        return nread;
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            return -EBADEXE;
    }

    if (ehdr.e_type != ET_DYN ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            return -EBADEXE;
    }

    int ret = 0;
    if ((ret = lseek(fd, ehdr.e_phoff, SEEK_SET)) < 0) {
        close(fd);
        return ret;
    }

    Elf64_Phdr* phdrs = malloc(sizeof(*phdrs) * ehdr.e_phnum);
    if (!phdrs) {
        close(fd);
        return -ENOMEM;
    }

    u64 load_high = USER_START;
    u64 load_low = USER_END;
    usize nloads = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != ehdr.e_phentsize) {
            free(phdrs);
            close(fd);
            return nread;
        }

        u64 seg_vaddr = base + phdrs[i].p_vaddr;
        if ((seg_vaddr + phdrs[i].p_memsz) >= USER_END) {
            free(phdrs);
            close(fd);
            return -ERANGE;
        }

        if (seg_vaddr + phdrs[i].p_memsz > load_high) load_high = seg_vaddr + phdrs[i].p_memsz;
        if (seg_vaddr < load_low) load_low  = seg_vaddr;
    }

    segment_ld_t* segs = malloc(sizeof(*segs) * nloads);
    if (!segs) {
        free(phdrs);
        close(fd);
        return -ENOMEM;
    }
    usize nldsegs = 0;

    u64 dyn_base = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD || phdrs[i].p_type == PT_DYNAMIC) {
            segment_ld_t seg = load_segment(&phdrs[i], fd, nasp, base);
            if (seg.code < 0) {
                clrsegs_err();
                free(segs);
                close(fd);
                return seg.code;
            }

            if (phdrs[i].p_type == PT_DYNAMIC) {
                dyn_base = base + phdrs[i].p_vaddr;
            } else {
                segs[nldsegs++] = seg;
            }
        }
    }

    if (!dyn_base) {
        return -EBADEXE;
    }

    if ((ret = processdyn(base, dyn_base, nasp)) < 0) {
        return ret;
    }

    close(fd);
    if (ldsz) *ldsz = load_high - load_low;
    return 0;
}

loadprog_res_t load_program(const char* path, char** argv, char** environ) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        kprint("Loader: failed to open file %s\n", path);
        return LOADPROG_ERR(fd);
    }

    kprint("Loading program %s\n", path);

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread < 0 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        kprint("Loader: failed to read ehdr\n");
        return LOADPROG_ERR(nread);
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            kprint("Loader: invalid or unsupported file\n");
            return LOADPROG_ERR(-EBADEXE);
    }

    // accept both static (ET_EXEC) and position-independent (ET_DYN) elfs
    if ((ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            kprint("Loader: invalid or unsupported file\n");
            return LOADPROG_ERR(-EBADEXE);
    }

    int ret = 0;
    if ((ret = lseek(fd, ehdr.e_phoff, SEEK_SET)) < 0) {
        close(fd);
        kprint("Loader: failed to get phdrs\n");
        return LOADPROG_ERR(ret);
    }

    Elf64_Phdr* phdrs = malloc(sizeof(*phdrs) * ehdr.e_phnum);
    if (!phdrs) {
        close(fd);
        return LOADPROG_ERR(-ENOMEM);
    }

    u64 load_high = USER_START;
    u64 load_low = USER_END;
    usize nloads = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != ehdr.e_phentsize) {
            free(phdrs);
            close(fd);
            kprint("Loader: failed to read phdrs\n");
            return LOADPROG_ERR(nread);
        }

        u64 seg_vaddr = phdrs[i].p_vaddr;
        if ((seg_vaddr + phdrs[i].p_memsz) >= USER_END) {
            free(phdrs);
            close(fd);
            kprint("Loader: program tried to load to invalid address\n");
            return LOADPROG_ERR(-ERANGE);
        }

        if (seg_vaddr + phdrs[i].p_memsz > load_high) load_high = seg_vaddr + phdrs[i].p_memsz;
        if (seg_vaddr < load_low)  load_low  = seg_vaddr;
        if (phdrs[i].p_type == PT_LOAD || phdrs[i].p_type == PT_LOAD) nloads++;
    }

    page_table_t* nasp = vmm_casp();
    segment_ld_t* segs = malloc(sizeof(*segs) * nloads);
    if (!segs) {
        free(phdrs);
        close(fd);
        return LOADPROG_ERR(-ENOMEM);
    }

    usize dyn_base = 0;

    usize nldsegs = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD || phdrs[i].p_type == PT_DYNAMIC) {
            segment_ld_t seg = load_segment(&phdrs[i], fd, nasp, 0x00);
            if (seg.code < 0) {
                clrsegs_err();
                free(segs);
                free(phdrs);
                close(fd);
                return LOADPROG_ERR(seg.code);
            }

            if (phdrs[i].p_type == PT_DYNAMIC) {
                dyn_base = phdrs[i].p_vaddr;
            } else {
                segs[nldsegs++] = seg;
            }
        } else if (phdrs[i].p_type == PT_INTERP) {
            char* interp = malloc(phdrs[i].p_filesz);
            if (!interp) {
                clrsegs_err();
                close(fd);
                return LOADPROG_ERR(-ENOMEM);
            }
            nread = read(fd, interp, phdrs[i].p_filesz);
            if (nread < 0 || (usize)nread != phdrs[i].p_filesz) {
                clrsegs_err();
                free(interp);
                close(fd);
                return LOADPROG_ERR(nread);
            }

            if (!streq(interp, "kernel")) {
                kprint("Aborting due to requested interpreter not kernel\n");
                clrsegs_err();
                free(interp);
                close(fd);
                return LOADPROG_ERR(-EBADEXE);
            }

            free(interp);
        }
    }
    free(phdrs);

    lodbase = (load_high + 0xFFF) & ~0xFFFULL;
    if (dyn_base) {
        if ((ret = processdyn(0, dyn_base, nasp)) < 0) {
            clrsegs_err();
            close(fd);
            return LOADPROG_ERR(ret);
        }

        for (usize i = 0; i < nloaded; i++) {
            for (usize i = 0; i < loaded[i].nldsegs; i++) {
                vmm_unmap_pages(vmm_cpml4v(), (u64)loaded[i].segs[i].ptr, loaded[i].segs[i].npgs, UNMAP_KEEPPHYS);
            }
        }

        memset(loaded, 0, sizeof(loaded));
        nloaded = 0;
        load_high = lodbase;
    }

    close(fd);

    u64 entry = ehdr.e_entry;
    u64 rsp = USER_END;

    void* stkptr = vmm_map_pages(vmm_cpml4v(), rsp - USTACK, 0, USTACKPGS, MAP_ANYPHYS | PAGE_WRITE | MAP_CONT);
    if (!stkptr) {
        kprint("Loader: failed to allocate the user stack\n");
        return LOADPROG_ERR(-ENOMEM);
    }

    u64 rsp_cpy = rsp;

    int ac = 0;
    while (argv[ac] != NULL && ac < ARGMAX) {
        ac++;
    }

    usize nenv = 0;
    while (environ[nenv] != NULL) {
        nenv++;
    }

    u64* evaddrs = malloc(sizeof(*evaddrs) * (nenv + 1));
    if (!evaddrs) {
        return LOADPROG_ERR(-ENOMEM);
    }
    for (int i = nenv - 1; i >= 0; i--) {
        usize len = strlen(environ[i]) + 1;
        rsp_cpy -= (u64)len;
        memcpy((void*)rsp_cpy, environ[i], len);
        evaddrs[i] = rsp_cpy;
    }

    u64* avaddrs = calloc(ARGMAX + 1, sizeof(*avaddrs));
    if (!avaddrs) {
        free(evaddrs);
        return LOADPROG_ERR(-ENOMEM);
    }
    for (int i = ac - 1; i >= 0; i--) {
        usize len = strlen(argv[i]) + 1;
        rsp_cpy -= (u64)len;
        memcpy((void*)rsp_cpy, argv[i], len);
        avaddrs[i] = rsp_cpy;
    }

    rsp_cpy &= ~15;

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = 0;

    for (int i = nenv - 1; i >= 0; i--) {
        rsp_cpy -= sizeof(u64);
        *(u64*)rsp_cpy = (u64)evaddrs[i];
    }

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = 0;

    for (int i = ac - 1; i >= 0; i--) {
        rsp_cpy -= sizeof(u64);
        *(u64*)rsp_cpy = (u64)avaddrs[i];
    }

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = (u64)ac;

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)(rsp - USTACK));
    if (!vmm_map_pages(nasp, rsp - USTACK, paddr, USTACKPGS, MAP_CONT | PAGE_USER | PAGE_WRITE)) {
        kprint("Loader: failed to map stack\n");
        return LOADPROG_ERR(-ENOMEM);
    }
    vmm_unmap_pages(vmm_cpml4v(), (u64)(rsp - USTACK), USTACKPGS, UNMAP_KEEPPHYS);

    free(avaddrs);
    free(evaddrs);
    free(segs);

    return (loadprog_res_t){
        .status = 0,
        .pgtbl = nasp,
        .entry = entry,
        .rsp = rsp_cpy,
        .load_high = load_high
    };
}