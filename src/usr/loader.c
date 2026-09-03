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
    usize size;
    Elf64_Sym* dynsym;
    usize nsyms;
    char* dynstr;
} dyninfo_t;
dyninfo_t loaded_libs[MAX_LIBRARIES];
dyninfo_t loaded_exe;
usize nloaded = 0;

#define MSR_KERNEL_GS_BASE 0xC0000102
extern __align(16) u8 kern_stack[65536];
static u64 gsblk[2];
void reset_kgsb() {
    gsblk[0] = 0x00007FFFFFFFF000;
    gsblk[1] = (u64)kern_stack + sizeof(kern_stack);
    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
}

u64 locate_extern(const char* name, usize* sz) {
    for (usize l = 0; l < nloaded; l++) {
        dyninfo_t* info = &loaded_libs[l];
        for (usize s = 0; s < info->nsyms; s++) {
            Elf64_Sym* sym = &info->dynsym[s];
            if (sym->st_shndx == SHN_UNDEF) {
                continue;
            }

            const char* symnam = info->dynstr + sym->st_name;
            if (streq(name, symnam)) {
                if (sz) *sz = sym->st_size;
                return info->base + sym->st_value;
            }
        }
    }

    for (usize s = 0; s < loaded_exe.nsyms; s++) {
        Elf64_Sym* sym = &loaded_exe.dynsym[s];
        if (sym->st_shndx == SHN_UNDEF) {
            continue;
        }

        const char* symnam = loaded_exe.dynstr + sym->st_name;
        if (streq(name, symnam)) {
            if (sz) *sz = sym->st_size;
            return loaded_exe.base + sym->st_value;
        }
    }
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

            memcpy((void*)rela->r_offset, (void*)symaddr, sz);
        }
        default: return 0;
    }

    //kprint("Relocating %p => %p\n", tgt_vaddr, v2r);

    *((u64*)tgt_vaddr) = v2r;
    return 0;
}

typedef struct {
    int code;
    dyninfo_t info;
} loadlib_res_t;
#define LOADLIB_ERR(ERRNO) ((loadlib_res_t){ERRNO,{0,0,NULL,0,NULL}})

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

loadlib_res_t load_library(const char* path, u64 base, page_table_t* nasp) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        return LOADLIB_ERR(fd);
    }

    kprint("Loading library %s at base %lu\n", path, base);

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread < 0 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        return LOADLIB_ERR(nread);
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            return LOADLIB_ERR(-EBADEXE);
    }

    if (ehdr.e_type != ET_DYN ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            return LOADLIB_ERR(-EBADEXE);
    }

    int ret = 0;
    if ((ret = lseek(fd, ehdr.e_phoff, SEEK_SET)) < 0) {
        close(fd);
        return LOADLIB_ERR(ret);
    }

    Elf64_Phdr phdrs[ehdr.e_phnum];
    u64 load_high = USER_START;
    u64 load_low = USER_END;
    usize nloads = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != ehdr.e_phentsize) {
            close(fd);
            return LOADLIB_ERR(nread);
        }

        u64 seg_vaddr = base + phdrs[i].p_vaddr;
        if ((seg_vaddr + phdrs[i].p_memsz) >= USER_END) {
            close(fd);
            return LOADLIB_ERR(-ERANGE);
        }

        if (seg_vaddr + phdrs[i].p_memsz > load_high) load_high = seg_vaddr + phdrs[i].p_memsz;
        if (seg_vaddr < load_low) load_low  = seg_vaddr;
        if (phdrs[i].p_type == PT_LOAD) nloads++;
    }

    segment_ld_t segs[nloads];
    usize nldsegs = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            segment_ld_t seg = load_segment(&phdrs[i], fd, nasp, base);
            if (seg.code < 0) {
                clrksegs(segs, nldsegs);
                close(fd);
                return LOADLIB_ERR(seg.code);
            }
            segs[nldsegs++] = seg;
        }
    }

    Elf64_Shdr* shdrs = malloc(sizeof(Elf64_Shdr) * ehdr.e_shnum);
    if (!shdrs) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADLIB_ERR(-ENOMEM);
    }

    if ((ret = lseek(fd, ehdr.e_shoff, SEEK_SET)) < 0) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADLIB_ERR(ret);
    }

    for (int i = 0; i < ehdr.e_shnum; i++) {
        ssize nread = read(fd, &shdrs[i], sizeof(Elf64_Shdr));
        if (nread < 0 || (usize)nread != ehdr.e_shentsize) {
            clrksegs(segs, nldsegs);
            free(shdrs);
            close(fd);
            return LOADLIB_ERR(nread);
        }
    }

    char* shstrtab = NULL;
    if (ehdr.e_shnum > 0 && ehdr.e_shstrndx != SHN_UNDEF) {
        Elf64_Shdr shstrtab_shdr = shdrs[ehdr.e_shstrndx];
        shstrtab = malloc(shstrtab_shdr.sh_size);
        if (!shstrtab) {
            clrksegs(segs, nldsegs);
            free(shdrs);
            close(fd);
            return LOADLIB_ERR(-ENOMEM);
        }

        if ((ret = readoff(fd, shstrtab, shstrtab_shdr.sh_size, shstrtab_shdr.sh_offset)) < 0) {
            clrksegs(segs, nldsegs);
            close(fd);
            free(shstrtab);
            free(shdrs);
            return LOADLIB_ERR(ret);
        }
    }

    Elf64_Shdr *dynsym_shdr = NULL, *dynstr_shdr = NULL;
    Elf64_Shdr *rela_dyn_shdr = NULL, *rela_plt_shdr = NULL;
    for (usize i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr* shdr = &shdrs[i];
        if (streq((char*)&shstrtab[shdr->sh_name], ".rela.plt")) {
            rela_plt_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".rela.dyn")) {
            rela_dyn_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".dynsym")) {
            dynsym_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".dynstr")) {
            dynstr_shdr = shdr;
        }
    }
    free(shstrtab);

    Elf64_Sym* dynsym = NULL;
    char* dynstr = NULL;

    if (!dynsym_shdr || !dynstr_shdr) { // both are required by ELF spec
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADLIB_ERR(-EBADEXE);
    }

    dynsym = malloc(dynsym_shdr->sh_size);
    if (!dynsym) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADLIB_ERR(-ENOMEM);
    }

    dynstr = malloc(dynstr_shdr->sh_size);
    if (!dynstr) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADLIB_ERR(-ENOMEM);
    }

    if ((ret = readoff(fd, dynsym, dynsym_shdr->sh_size, dynsym_shdr->sh_offset)) < 0) {
        clrksegs(segs, nldsegs);
        free(dynsym);
        free(dynstr);
        close(fd);
        return LOADLIB_ERR(ret);
    }

    if ((ret = readoff(fd, dynstr, dynstr_shdr->sh_size, dynstr_shdr->sh_offset)) < 0) {
        clrksegs(segs, nldsegs);
        free(dynsym);
        free(dynstr);
        close(fd);
        return LOADLIB_ERR(ret);
    }

    // R_X86_64_RELATIVE
    // R_X86_64_GLOB_DAT
    // R_X86_64_JUMP_SLOT

    dyninfo_t info = {
        .base = base,
        .size = load_high - load_low,
        .dynsym = dynsym,
        .nsyms = dynsym_shdr->sh_size / sizeof(Elf64_Sym),
        .dynstr = dynstr
    };

    if (rela_dyn_shdr) {
        Elf64_Rela* relas = malloc(rela_dyn_shdr->sh_size);
        if (!relas) {
            clrksegs(segs, nldsegs);
            free(dynsym);
            free(dynstr);
            close(fd);
            return LOADLIB_ERR(-ENOMEM);
        }

        if ((ret = readoff(fd, relas, rela_dyn_shdr->sh_size, rela_dyn_shdr->sh_offset)) < 0) {
            clrksegs(segs, nldsegs);
            free(relas);
            free(dynsym);
            free(dynstr);
            close(fd);
            return LOADLIB_ERR(ret);
        }

        usize nrels = rela_dyn_shdr->sh_size / sizeof(Elf64_Rela);
        for (usize i = 0; i < nrels; i++) {
            if ((ret = apply_rela(&relas[i], &info)) < 0) {
                clrksegs(segs, nldsegs);
                free(relas);
                free(dynsym);
                free(dynstr);
                close(fd);
                kprint("Failed to apply relocations while loading library %s\n", path);
                return LOADLIB_ERR(ret);
            }
        }

        free(relas);
    }

    if (rela_plt_shdr) {
        Elf64_Rela* relas = malloc(rela_plt_shdr->sh_size);
        if (!relas) {
            clrksegs(segs, nldsegs);
            free(dynsym);
            free(dynstr);
            close(fd);
            return LOADLIB_ERR(-ENOMEM);
        }

        if ((ret = readoff(fd, relas, rela_plt_shdr->sh_size, rela_plt_shdr->sh_offset)) < 0) {
            clrksegs(segs, nldsegs);
            free(relas);
            free(dynsym);
            free(dynstr);
            close(fd);
            return LOADLIB_ERR(ret);
        }

        usize nrels = rela_plt_shdr->sh_size / sizeof(Elf64_Rela);
        for (usize i = 0; i < nrels; i++) {
            if ((ret = apply_rela(&relas[i], &info)) < 0) {
                clrksegs(segs, nldsegs);
                free(relas);
                free(dynsym);
                free(dynstr);
                close(fd);
                kprint("Failed to apply relocations while loading library %s\n", path);
                return LOADLIB_ERR(ret);
            }
        }

        free(relas);
    }

    clrksegs(segs, nldsegs);
    free(shdrs);
    close(fd);
    return (loadlib_res_t){0, info};
}

int program_processdyn(int fd, u64 load_low, u64* load_high, Elf64_Ehdr* ehdr,
                       Elf64_Shdr* shdrs, char* shstrtab, page_table_t* nasp) {
    usize ndyns = 0;
    Elf64_Shdr* dynshdr = NULL;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr* shdr = &shdrs[i];
        if (shdr->sh_type == SHT_DYNAMIC) {
            ndyns = shdr->sh_size / sizeof(Elf64_Dyn);
            dynshdr = shdr;
            break;
        }
    }

    Elf64_Shdr *dynsym_shdr = NULL, *dynstr_shdr = NULL;
    Elf64_Shdr *rela_dyn_shdr = NULL, *rela_plt_shdr = NULL;
    for (usize i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr* shdr = &shdrs[i];
        if (streq((char*)&shstrtab[shdr->sh_name], ".rela.plt")) {
            rela_plt_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".rela.dyn")) {
            rela_dyn_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".dynsym")) {
            dynsym_shdr = shdr;
        } else if (streq((char*)&shstrtab[shdr->sh_name], ".dynstr")) {
            dynstr_shdr = shdr;
        }
    }

    Elf64_Sym* dynsym = NULL;
    char* dynstr = NULL;

    if (!dynsym_shdr || !dynstr_shdr) { // both are required by ELF spec
        return -EBADEXE;
    }

    dynsym = malloc(dynsym_shdr->sh_size);
    if (!dynsym) {
        return -ENOMEM;
    }

    dynstr = malloc(dynstr_shdr->sh_size);
    if (!dynstr) {
        return -ENOMEM;
    }

    int ret = 0;
    if ((ret = readoff(fd, dynsym, dynsym_shdr->sh_size, dynsym_shdr->sh_offset)) < 0) {
        free(dynsym);
        free(dynstr);
        return ret;
    }

    if ((ret = readoff(fd, dynstr, dynstr_shdr->sh_size, dynstr_shdr->sh_offset)) < 0) {
        free(dynsym);
        free(dynstr);
        return ret;
    }

    u64 lib_base = (*load_high + 0xFFF) & ~0xFFFULL;

    loaded_exe = (dyninfo_t){
        .base = (u64)load_low,
        .size = (usize)load_high - (usize)load_low,
        .dynsym = dynsym,
        .nsyms = dynsym_shdr->sh_size / sizeof(Elf64_Sym),
        .dynstr = dynstr
    };

    if (dynshdr && ndyns > 0) {
        if ((ret = lseek(fd, dynshdr->sh_offset, SEEK_SET)) < 0) {
            free(dynsym);
            free(dynstr);
            return ret;
        }
        Elf64_Dyn dyns[ndyns];
        ssize nread = read(fd, dyns, dynshdr->sh_size);
        if (nread < 0 || (usize)nread != dynshdr->sh_size) {
            free(dynsym);
            free(dynstr);
            return nread;
        }

        for (usize i = 0; i < ndyns; i++) {
            if (dyns[i].d_tag == DT_NEEDED) {
                if (nloaded + 1 > MAX_LIBRARIES) {
                    break;
                }

                loadlib_res_t res = load_library(dynstr + dyns[i].d_un.d_ptr, lib_base, nasp);
                if (res.code < 0) {
                    free(dynsym);
                    free(dynstr);
                    return res.code;
                } else {
                    loaded_libs[nloaded++] = res.info;
                    lib_base += (res.info.size + 0xFFF) & ~0xFFFULL;;
                }
            }
        }
    }

    dyninfo_t info = {
        .base = 0,
        .size = *load_high - load_low,
        .dynsym = dynsym,
        .nsyms = dynsym_shdr->sh_size / sizeof(Elf64_Sym),
        .dynstr = dynstr
    };

    if (rela_dyn_shdr != NULL) {
        Elf64_Rela* relas = malloc(rela_dyn_shdr->sh_size);
        if (!relas) {
            free(dynsym);
            free(dynstr);
            return -ENOMEM;
        }

        if ((ret = readoff(fd, relas, rela_dyn_shdr->sh_size, rela_dyn_shdr->sh_offset)) < 0) {
            free(relas);
            free(dynsym);
            free(dynstr);
            return ret;
        }

        usize nrels = rela_dyn_shdr->sh_size / sizeof(Elf64_Rela);
        for (usize i = 0; i < nrels; i++) {
            if ((ret = apply_rela(&relas[i], &info)) < 0) {
                free(relas);
                free(dynsym);
                free(dynstr);
                kprint("Failed to apply relocations while loading program\n");
                return ret;
            }
        }

        free(relas);
    }

    if (rela_plt_shdr != NULL) {
        Elf64_Rela* relas = malloc(rela_plt_shdr->sh_size);
        if (!relas) {
            free(dynsym);
            free(dynstr);
            return -ENOMEM;
        }

        if ((ret = readoff(fd, relas, rela_plt_shdr->sh_size, rela_plt_shdr->sh_offset)) < 0) {
            free(relas);
            free(dynsym);
            free(dynstr);
            return ret;
        }

        usize nrels = rela_plt_shdr->sh_size / sizeof(Elf64_Rela);
        for (usize i = 0; i < nrels; i++) {
            if ((ret = apply_rela(&relas[i], &info)) < 0) {
                free(relas);
                free(dynsym);
                free(dynstr);
                kprint("Failed to apply relocations while loading program\n");
                return ret;
            }
        }

        free(relas);
    }

    // free dynamically allocated structures after all relocations
    free(dynsym);
    free(dynstr);
    for (usize i = 0; i < nloaded; i++) {
        free(loaded_libs[i].dynstr);
        free(loaded_libs[i].dynsym);
    }
    memset(loaded_libs, 0, sizeof(loaded_libs));
    memset(&loaded_exe, 0, sizeof(loaded_exe));
    nloaded = 0;
    *load_high = lib_base;

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

    Elf64_Phdr phdrs[ehdr.e_phnum];
    u64 load_high = USER_START;
    u64 load_low = USER_END;
    usize nloads = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != ehdr.e_phentsize) {
            close(fd);
            kprint("Loader: failed to read phdrs\n");
            return LOADPROG_ERR(nread);
        }

        u64 seg_vaddr = phdrs[i].p_vaddr;
        if ((seg_vaddr + phdrs[i].p_memsz) >= USER_END) {
            close(fd);
            kprint("Loader: program tried to load to invalid address\n");
            return LOADPROG_ERR(-ERANGE);
        }

        if (seg_vaddr + phdrs[i].p_memsz > load_high) load_high = seg_vaddr + phdrs[i].p_memsz;
        if (seg_vaddr < load_low)  load_low  = seg_vaddr;
        if (phdrs[i].p_type == PT_LOAD) nloads++;
    }

    int is_dyn = 0;
    page_table_t* nasp = vmm_casp();
    segment_ld_t segs[nloads];
    usize nldsegs = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            segment_ld_t seg = load_segment(&phdrs[i], fd, nasp, 0x00);
            if (seg.code < 0) {
                clrksegs(segs, nldsegs);
                close(fd);
                return LOADPROG_ERR(seg.code);
            }
            segs[nldsegs++] = seg;
        } else if (phdrs[i].p_type == PT_INTERP) {
            char* interp = malloc(phdrs[i].p_filesz);
            if (!interp) {
                clrksegs(segs, nldsegs);
                close(fd);
                return LOADPROG_ERR(-ENOMEM);
            }
            nread = read(fd, interp, phdrs[i].p_filesz);
            if (nread < 0 || (usize)nread != phdrs[i].p_filesz) {
                clrksegs(segs, nldsegs);
                free(interp);
                close(fd);
                return LOADPROG_ERR(nread);
            }

            if (!streq(interp, "kernel")) {
                kprint("Aborting due to requested interpreter not kernel\n");
                clrksegs(segs, nldsegs);
                free(interp);
                close(fd);
                return LOADPROG_ERR(-EBADEXE);
            }

            free(interp);
        } else if (phdrs[i].p_type == PT_DYNAMIC) {
            is_dyn = 1;
        }
    }

    if ((ret = lseek(fd, ehdr.e_shoff, SEEK_SET)) < 0) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADPROG_ERR(ret);
    }

    Elf64_Shdr* shdrs = malloc(sizeof(Elf64_Shdr) * ehdr.e_shnum);
    if (!shdrs) {
        clrksegs(segs, nldsegs);
        close(fd);
        return LOADPROG_ERR(-ENOMEM);
    }

    for (int i = 0; i < ehdr.e_shnum; i++) {
        ssize nread = read(fd, &shdrs[i], sizeof(Elf64_Shdr));
        if (nread < 0 || (usize)nread != ehdr.e_shentsize) {
            clrksegs(segs, nldsegs);
            free(shdrs);
            close(fd);
            return LOADPROG_ERR(nread);
        }
    }

    char* shstrtab = NULL;
    if (ehdr.e_shnum > 0 && ehdr.e_shstrndx != SHN_UNDEF) {
        Elf64_Shdr shstrtab_shdr = shdrs[ehdr.e_shstrndx];
        shstrtab = malloc(shstrtab_shdr.sh_size);
        if (!shstrtab) {
            clrksegs(segs, nldsegs);
            free(shdrs);
            close(fd);
            return LOADPROG_ERR(-ENOMEM);
        }

        if ((ret = lseek(fd, shstrtab_shdr.sh_offset, SEEK_SET)) < 0) {
            clrksegs(segs, nldsegs);
            close(fd);
            free(shdrs);
            free(shstrtab);
            return LOADPROG_ERR(ret);
        }

        nread = read(fd, shstrtab, shstrtab_shdr.sh_size);
        if (nread < 0 || (usize)nread != shstrtab_shdr.sh_size) {
            clrksegs(segs, nldsegs);
            close(fd);
            free(shdrs);
            free(shstrtab);
            return LOADPROG_ERR(nread);
        }
    }

    if (is_dyn) {
        if ((ret = program_processdyn(fd, load_low, &load_high, &ehdr, shdrs, shstrtab, nasp)) < 0) {
            clrksegs(segs, nldsegs);
            close(fd);
            free(shdrs);
            free(shstrtab);
            return LOADPROG_ERR(ret);
        }
    }

    clrksegs(segs, nldsegs);
    free(shstrtab);
    free(shdrs);
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

    u64 evaddrs[nenv + 1];
    for (int i = nenv - 1; i >= 0; i--) {
        usize len = strlen(environ[i]) + 1;
        rsp_cpy -= (u64)len;
        memcpy((void*)rsp_cpy, environ[i], len);
        evaddrs[i] = rsp_cpy;
    }

    u64 avaddrs[ARGMAX + 1] = {0};
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

    return (loadprog_res_t){
        .status = 0,
        .pgtbl = nasp,
        .entry = entry,
        .rsp = rsp_cpy,
        .load_high = load_high
    };
}