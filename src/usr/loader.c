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

#define USTACK    (128 * 4096)
#define USTACKPGS 128
#define ARGMAX    16

extern u64 ram_max;

typedef struct {
    int code;
    void* ptr;
    usize npgs;
} segment_ld_t;
#define SEGLD_ERR(ERRNO) ((segment_ld_t){ERRNO,NULL,0})

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

#define MSR_KERNEL_GS_BASE 0xC0000102
extern __align(16) u8 kern_stack[65536];
static u64 gsblk[2];
void reset_kgsb() {
    gsblk[0] = 0x00007FFFFFFFF000;
    gsblk[1] = (u64)kern_stack + sizeof(kern_stack);
    wrmsr(MSR_KERNEL_GS_BASE, (u64)&gsblk);
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

void clear_segs(segment_ld_t* segs, usize nsegs) {
    for (usize i = 0; i < nsegs; i++) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)segs[i].ptr, segs[i].npgs, 0);
    }
}

typedef struct {
    usize nldsegs;
    segment_ld_t* segs;
    int has_interp;
    usize interp_phdrndx;
    u64 phdrs_vaddr;
    u64 ldhigh;
    Elf64_Ehdr ehdr;
} loadinfo_t;

int loadexe_base(const char* path, u64 base, page_table_t* nasp, loadinfo_t* info) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        kprint("Loader: failed to open file %s\n", path);
        return fd;
    }

    kprint("Loading program %s\n", path);

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread < 0 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        kprint("Loader: failed to read ehdr\n");
        if (nread < 0) return nread;
        return -EBADEXE;
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            kprint("Loader: invalid or unsupported file\n");
            return -EBADEXE;
    }

    if ((ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            kprint("Loader: invalid or unsupported file\n");
            return -EBADEXE;
    }

    if (ehdr.e_type == ET_DYN && base == 0) {
        base = 0x200000;
    }

    info->ehdr = ehdr;

    int ret = 0;
    if ((ret = lseek(fd, ehdr.e_phoff, SEEK_SET)) < 0) {
        close(fd);
        kprint("Loader: failed to get phdrs\n");
        return ret;
    }

    Elf64_Phdr* phdrs = malloc(sizeof(*phdrs) * ehdr.e_phnum);
    if (!phdrs) {
        close(fd);
        return -ENOMEM;
    }

    usize nloads = 0;
    info->ldhigh = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread < 0 || (usize)nread != ehdr.e_phentsize) {
            free(phdrs);
            close(fd);
            kprint("Loader: failed to read phdrs\n");
            if (nread < 0) return nread;
            return -EBADEXE;
        }

        u64 seg_vaddr = phdrs[i].p_vaddr + base;
        u64 seghigh = seg_vaddr + phdrs[i].p_memsz;
        if (seghigh >= USER_END) {
            free(phdrs);
            close(fd);
            kprint("Loader: program tried to load to invalid address\n");
            return -ERANGE;
        }

        if (seghigh > info->ldhigh) info->ldhigh = seghigh;
        if (phdrs[i].p_type == PT_LOAD) nloads++;
    }

    segment_ld_t* segs = malloc(sizeof(*segs) * nloads);
    if (!segs) {
        free(phdrs);
        close(fd);
        return -ENOMEM;
    }

    info->nldsegs = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            segment_ld_t seg = load_segment(&phdrs[i], fd, nasp, base);
            if (seg.code < 0) {
                for (usize i = 0; i < info->nldsegs; i++) {
                    vmm_unmap_pages(vmm_cpml4v(), (u64)segs[i].ptr, segs[i].npgs, 0);
                }
                free(segs);
                free(phdrs);
                close(fd);
                return seg.code;
            }

            segs[info->nldsegs++] = seg;
        } else if (phdrs[i].p_type == PT_INTERP) {
            info->has_interp = 1;
            info->interp_phdrndx = i;
        } else if (phdrs[i].p_type == PT_PHDR) {
            info->phdrs_vaddr = base + phdrs[i].p_vaddr;
        }
    }

    info->segs = segs;

    free(phdrs);
    phdrs = NULL;
    close(fd);

    return 0;
}

loadprog_res_t load_program(const char* path, char** argv, char** environ) {
    int ret = 0;
    loadinfo_t exe_info = {0};
    page_table_t* nasp = vmm_casp();

    if ((ret = loadexe_base(path, 0, nasp, &exe_info)) < 0) {
        vmm_dasp(nasp);
        return LOADPROG_ERR(ret);
    }

    Elf64_Auxv auxv[NAUXV] = {0};
    auxv[0] = (Elf64_Auxv){AT_PHDR, exe_info.phdrs_vaddr};
    auxv[1] = (Elf64_Auxv){AT_PHNUM, exe_info.ehdr.e_phnum};
    auxv[2] = (Elf64_Auxv){AT_ENTRY, exe_info.ehdr.e_entry};
    u64 entry = exe_info.ehdr.e_entry;
    u64 ldhigh = exe_info.ldhigh;

    if (exe_info.has_interp) {
        Elf64_Phdr* ephdrs = (Elf64_Phdr*)exe_info.phdrs_vaddr;
        char* interp = (char*)(ephdrs[exe_info.interp_phdrndx].p_vaddr);

        u64 intrp_base = ((exe_info.ldhigh + 1) + 0xFFFULL) & ~0xFFFULL;
        loadinfo_t intrp_info = {0};

        if ((ret = loadexe_base(interp, intrp_base, nasp, &intrp_info)) < 0) {
            for (usize i = 0; i < exe_info.nldsegs; i++) {
                vmm_unmap_pages(vmm_cpml4v(), (u64)exe_info.segs[i].ptr, exe_info.segs[i].npgs, 0);
            }
            vmm_dasp(nasp);
        }

        for (usize i = 0; i < intrp_info.nldsegs; i++) {
            vmm_unmap_pages(vmm_cpml4v(), (u64)intrp_info.segs[i].ptr, intrp_info.segs[i].npgs, 0);
        }

        auxv[3] = (Elf64_Auxv){AT_IPHDRS, intrp_info.phdrs_vaddr};
        auxv[4] = (Elf64_Auxv){AT_IPHNUM, intrp_info.ehdr.e_phnum};
        entry = intrp_base + intrp_info.ehdr.e_entry;
        ldhigh = intrp_info.ldhigh;
    }

    for (usize i = 0; i < exe_info.nldsegs; i++) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)exe_info.segs[i].ptr, exe_info.segs[i].npgs, 0);
    }

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

    rsp_cpy -= sizeof(Elf64_Auxv) * 8;
    Elf64_Auxv* stk_auxv = (Elf64_Auxv*)rsp_cpy;
    memcpy(stk_auxv, auxv, sizeof(Elf64_Auxv) * 5);
    stk_auxv[5] = (Elf64_Auxv){AT_STACK, rsp};
    stk_auxv[6] = (Elf64_Auxv){AT_STACKSZ, USTACK};
    stk_auxv[7] = (Elf64_Auxv){AT_NULL, 0};

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)(rsp - USTACK));
    if (!vmm_map_pages(nasp, rsp - USTACK, paddr, USTACKPGS, MAP_CONT | PAGE_USER | PAGE_WRITE)) {
        kprint("Loader: failed to map stack\n");
        return LOADPROG_ERR(-ENOMEM);
    }
    vmm_unmap_pages(vmm_cpml4v(), (u64)(rsp - USTACK), USTACKPGS, UNMAP_KEEPPHYS);

    free(avaddrs);
    free(evaddrs);

    return (loadprog_res_t){
        .status = 0,
        .pgtbl = nasp,
        .entry = entry,
        .rsp = rsp_cpy,
        .load_high = ldhigh
    };
}