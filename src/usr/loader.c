#include <core/elf.h>
#include <drivers/fs.h>
#include <lib/string.h>
#include <core/mem/vmm.h>
#include <core/liballoc.h>
#include <core/printf.h>
#include <drivers/term.h>
#include <lib/loader.h>
#include <lib/syscall.h>
#include <core/printf.h>

#define USTACK    (16 * 4096)
#define USTACKPGS 16
#define ARGMAX    16

#define DYN_LOAD_BASE 0x00400000

extern u64 ram_max;

int load_segment(Elf64_Phdr* phdr, int fd, page_table_t* nasp, u64 load_base) {
    u64 seg_vaddr = load_base + phdr->p_vaddr;

    usize npgs = phdr->p_memsz / 4096;
    if (phdr->p_memsz % 4096 != 0) npgs++;

    void* addr = vmm_map_pages(vmm_cpml4v(), USER_START + seg_vaddr, 0, npgs, MAP_ANYPHYS | MAP_CONT | PAGE_WRITE);
    if (!addr) return -1;

    if (lseek(fd, phdr->p_offset, SEEK_SET) < 0) {
        printf("Loader: failed to seek phdr offset\n");
        return -1;
    }

    if (phdr->p_memsz > phdr->p_filesz) {
        memset(addr, 0, phdr->p_memsz);
    }

    ssize nread = read(fd, addr, phdr->p_filesz);
    if (nread < 0 || (usize)nread < phdr->p_filesz) {
        printf("Loader: failed to read program data\n");
        return -1;
    }

    u64 flgs = PAGE_USER;
    if (phdr->p_flags & PF_W) {
        flgs |= PAGE_WRITE;
    }

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)addr);
    if (!vmm_map_pages(nasp, seg_vaddr, paddr, npgs, MAP_CONT | flgs)) {
        return -1;
    }

    vmm_unmap_pages(vmm_cpml4v(), (u64)addr, npgs, UNMAP_KEEPPHYS);
    return 0;
}

// handle dynamic relocations for PIE/shared-object elfs
// only does R_X86_64_RELATIVE for now, thats enough for simple PIE stuff
static int process_relocations(Elf64_Phdr* phdrs, int phnum, int fd,
                               page_table_t* nasp, u64 load_base) {
    // find PT_DYNAMIC
    Elf64_Phdr* dyn_phdr = NULL;
    for (int i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_phdr = &phdrs[i];
            break;
        }
    }
    if (!dyn_phdr) return 0; // nothing dynamic here

    if (dyn_phdr->p_filesz == 0) return 0;

    // read the dynamic table into a temp kernel buffer
    usize dyn_npgs = dyn_phdr->p_filesz / 4096;
    if (dyn_phdr->p_filesz % 4096 != 0) dyn_npgs++;

    void* dyn_buf = vmm_map_pages(vmm_cpml4v(), 0, 0,
                                  dyn_npgs, MAP_ANYPHYS | MAP_CONT | MAP_ANYVIRT | PAGE_WRITE);
    if (!dyn_buf) return -1;

    if (lseek(fd, dyn_phdr->p_offset, SEEK_SET) < 0) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)dyn_buf, dyn_npgs, 0);
        return -1;
    }

    ssize nread = read(fd, dyn_buf, dyn_phdr->p_filesz);
    if (nread < 0 || (usize)nread < dyn_phdr->p_filesz) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)dyn_buf, dyn_npgs, 0);
        return -1;
    }

    // pull out the rela info we need
    Elf64_Dyn* dyn = (Elf64_Dyn*)dyn_buf;
    u64 rela_off = 0, rela_sz = 0, rela_ent = 0;

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_RELA:    rela_off = dyn[i].d_un.d_ptr; break;
            case DT_RELASZ:  rela_sz  = dyn[i].d_un.d_val; break;
            case DT_RELAENT: rela_ent = dyn[i].d_un.d_val; break;
        }
    }

    vmm_unmap_pages(vmm_cpml4v(), (u64)dyn_buf, dyn_npgs, 0);

    if (!rela_off || !rela_sz || !rela_ent) return 0; // no relocs

    // rela_off is a vaddr in the elf, need to figure out where it
    // lives in the actual file by checking which PT_LOAD owns it
    u64 rela_file_off = 0;
    int found_rela_seg = 0;
    for (int i = 0; i < phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;
        u64 seg_start = phdrs[i].p_vaddr;
        u64 seg_end = seg_start + phdrs[i].p_filesz;
        if (rela_off >= seg_start && rela_off < seg_end) {
            rela_file_off = phdrs[i].p_offset + (rela_off - seg_start);
            found_rela_seg = 1;
            break;
        }
    }
    if (!found_rela_seg) return -1;

    usize rela_npgs = rela_sz / 4096;
    if (rela_sz % 4096 != 0) rela_npgs++;

    void* rela_buf = vmm_map_pages(vmm_cpml4v(), 0, 0,
                                   rela_npgs, MAP_ANYPHYS | MAP_CONT | MAP_ANYVIRT | PAGE_WRITE);
    if (!rela_buf) return -1;

    if (lseek(fd, rela_file_off, SEEK_SET) < 0) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)rela_buf, rela_npgs, 0);
        return -1;
    }

    nread = read(fd, rela_buf, rela_sz);
    if (nread < 0 || (usize)nread < rela_sz) {
        vmm_unmap_pages(vmm_cpml4v(), (u64)rela_buf, rela_npgs, 0);
        return -1;
    }

    // patch em in
    usize nrela = rela_sz / sizeof(Elf64_Rela);
    Elf64_Rela* rela = (Elf64_Rela*)rela_buf;

    for (usize i = 0; i < nrela; i++) {
        u32 type = ELF64_R_TYPE(rela[i].r_info);
        u64 target_vaddr = load_base + rela[i].r_offset;

        if (type == R_X86_64_RELATIVE) {
            // RELATIVE: just base + addend, easy
            // poke through HHDM since the page is in the user address space
            u64 target_page = target_vaddr & ~0xFFFULL;
            u64 target_off  = target_vaddr & 0xFFFULL;

            u64 target_phys = vmm_get_phys(nasp, target_page);
            if (!target_phys) continue;

            u64* patch = (u64*)(HHDM_START + target_phys + target_off);
            *patch = load_base + rela[i].r_addend;
        } else {
            // dunno what this is, skip it
            printf("ELF: unsupported relocation type %u at offset 0x%lx\n",
                   type, rela[i].r_offset);
        }
    }

    vmm_unmap_pages(vmm_cpml4v(), (u64)rela_buf, rela_npgs, 0);
    return 0;
}

int load_program(const char* path, char** argv) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Loader: failed to open file\n");
        return -1;
    }

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread == -1 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        printf("Loader: failed to read ehdr\n");
        return -1;
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            printf("Loader: invalid or unsupported file\n");
            return -1;
    }

    // we take both static (ET_EXEC) and dynamic/PIE (ET_DYN) elfs now
    if ((ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            printf("Loader: invalid or unsupported file\n");
            return -1;
    }

    int is_dyn = (ehdr.e_type == ET_DYN);
    u64 load_base = is_dyn ? DYN_LOAD_BASE : 0;

    if (lseek(fd, ehdr.e_phoff, SEEK_SET) < 0) {
        close(fd);
        printf("Loader: failed to get phdrs\n");
        return -1;
    }

    Elf64_Phdr phdrs[ehdr.e_phnum];
    u64 load_high = USER_START;
    u64 load_low = USER_END;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread == -1 || (usize)nread != ehdr.e_phentsize) {
            close(fd);
            printf("Loader: failed to read phdrs\n");
            return -1;
        }
        u64 seg_vaddr = load_base + phdrs[i].p_vaddr;
        if ((seg_vaddr + phdrs[i].p_memsz) >= USER_END) {
            close(fd);
            printf("Loader: program tried to load to invalid address\n");
            return -1;
        }

        if (seg_vaddr + phdrs[i].p_memsz > load_high) load_high = seg_vaddr + phdrs[i].p_memsz;
        if (seg_vaddr < load_low)  load_low  = seg_vaddr;
    }

    vmm_setumapbase(load_high);

    page_table_t* nasp = vmm_casp();
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (load_segment(&phdrs[i], fd, nasp, load_base) < 0) {
                close(fd);
                return -1;
            }
        }
    }

    // do relocations if this is a dynamic elf
    if (is_dyn) {
        if (process_relocations(phdrs, ehdr.e_phnum, fd, nasp, load_base) < 0) {
            printf("ELF: relocation processing failed\n");
            close(fd);
            return -1;
        }
    }

    close(fd);

    u64 entry = is_dyn ? (load_base + ehdr.e_entry) : ehdr.e_entry;
    u64 rsp = USER_END;

    void* stkptr = vmm_map_pages(vmm_cpml4v(), rsp - USTACK, 0, USTACKPGS, MAP_ANYPHYS | PAGE_WRITE | MAP_CONT);
    if (!stkptr) {
        return -1;
    }

    u64 rsp_cpy = rsp;

    int ac = 0;
    while (argv[ac] != NULL && ac < ARGMAX) {
        ac++;
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

    for (int i = ac - 1; i >= 0; i--) {
        rsp_cpy -= sizeof(u64);
        *(u64*)rsp_cpy = (u64)avaddrs[i];
    }

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = (u64)ac;

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)(rsp - USTACK));
    if (!vmm_map_pages(nasp, rsp - USTACK, paddr, USTACKPGS, MAP_CONT | PAGE_USER | PAGE_WRITE)) {
        printf("Loader: failed to map stack\n");
        return -1;
    }
    vmm_unmap_pages(vmm_cpml4v(), (u64)(rsp - USTACK), USTACKPGS, UNMAP_KEEPPHYS);
    init_syscalls();

    vmm_sasp(nasp);
    asm volatile(
        "cli\n\t"
        
        "mov $0x23, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"

        "pushq $0x23\n\t"
        "pushq %%rsi\n\t"
        
        "pushfq\n\t"
        "popq %%rax\n\t"
        "orq $0x200, %%rax\n\t"
        "pushq %%rax\n\t"
        
        "pushq $0x1b\n\t"
        "pushq %%rdi\n\t"

        "iretq\n\t"
        :
        : "D"(entry), "S"(rsp_cpy)
        : "rax", "memory"
    );

    return -1;    
}