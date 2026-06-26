#include <core/elf.h>
#include <drivers/fs.h>
#include <lib/string.h>
#include <core/mem/vmm.h>

#define USTACK    (64 * 1024)
#define USTACKPGS 16
#define ARGMAX    16

extern u64 ram_max;

int load_segment(Elf64_Phdr* phdr, int fd, page_table_t* nasp) {
    usize npgs = phdr->p_memsz / 4096;
    if (phdr->p_memsz % 4096 != 0) npgs++;

    void* addr = vmm_map_pages(vmm_cpml4v(), USER_START + phdr->p_vaddr, 0, npgs, MAP_ANYPHYS | MAP_CONT | PAGE_WRITE);
    if (!addr) return -1;

    if (lseek(fd, phdr->p_offset, SEEK_SET) < 0) {
        return -1;
    }

    if (phdr->p_memsz > phdr->p_filesz) {
        memset(addr, 0, phdr->p_memsz);
    }

    ssize nread = read(fd, addr, phdr->p_filesz);
    if (nread < 0 || (usize)nread < phdr->p_filesz) {
        return -1;
    }

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)addr);
    if (!vmm_map_pages(nasp, phdr->p_vaddr, paddr, npgs, MAP_CONT | PAGE_USER)) {
        return -1;
    }

    vmm_unmap_pages(vmm_cpml4v(), (u64)addr, npgs, UNMAP_KEEPPHYS);

    return 0;
}

int load_program(const char* path, char** argv) {
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    Elf64_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread == -1 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        return -1;
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            return -1;
    }

    if (ehdr.e_type    != ET_EXEC   ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            return -1;
    }

    if (lseek(fd, ehdr.e_phoff, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    Elf64_Phdr phdrs[ehdr.e_phnum];
    u64 load_high = USER_START;
    u64 load_low = USER_END;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf64_Phdr));
        if (nread == -1 || (usize)nread != ehdr.e_phentsize) {
            close(fd);
            return -1;
        }
        if (phdrs[i].p_vaddr < USER_START || (phdrs[i].p_vaddr + phdrs[i].p_memsz) >= USER_END) {
            close(fd);
            return -1;
        }

        if (phdrs[i].p_vaddr > load_high) load_high = phdrs[i].p_vaddr;
        if (phdrs[i].p_vaddr < load_low)  load_low  = phdrs[i].p_vaddr;
    }

    page_table_t* nasp = vmm_casp();
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (load_segment(&phdrs[i], fd, nasp) < 0) {
                close(fd);
                return -1;
            }
        }
    }

    close(fd);

    u64 rsp = 0x00007FFFFFFFF000;
    u64 ucode = 0x1B;
    u64 udata = 0x23;

    vmm_map_pages(vmm_cpml4v(), rsp - USTACK, 0, USTACKPGS, MAP_ANYPHYS | PAGE_WRITE | MAP_CONT);

    u64 rsp_cpy = rsp;

    int ac = 0;
    while (argv[ac] != NULL && ac < ARGMAX) {
        ac++;
    }

    u64 avaddrs[ARGMAX] = {0};

    for (int i = ac - 1; i >= 0; i--) {
        u32 len = strlen(argv[i]) + 1;
        rsp_cpy -= len;

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

    u64 vargvp = rsp_cpy;

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = vargvp;

    rsp_cpy -= sizeof(u64);
    *(u64*)rsp_cpy = (u64)ac;

    rsp_cpy &= ~15;

    u64 paddr = vmm_get_phys(vmm_cpml4v(), (u64)(rsp - USTACK));
    if (!vmm_map_pages(nasp, rsp - USTACK, paddr, USTACKPGS, MAP_CONT | PAGE_USER | PAGE_WRITE)) {
        return -1;
    }
    vmm_unmap_pages(vmm_cpml4v(), (u64)(rsp - USTACK), USTACKPGS, UNMAP_KEEPPHYS);
    vmm_sasp(nasp);
    asm volatile(
        "cli\n\t"
        
        "mov %0, %%ds\n\t"
        "mov %0, %%es\n\t"
        "mov %0, %%fs\n\t"
        "mov %0, %%gs\n\t"

        "pushq %0\n\t"
        "pushq %1\n\t"

        "pushfq\n\t"
        "popq %%rax\n\t"
        "orq $0x200, %%rax\n\t"
        "pushq %%rax\n\t"
        
        "pushq %2\n\t"
        "pushq %3\n\t"

        "iretq\n\t"

        :: "r"(udata), "r"(rsp_cpy), 
           "r"(ucode), "r"(ehdr.e_entry)
        : "rax", "memory"
    );

    return -1;    
}