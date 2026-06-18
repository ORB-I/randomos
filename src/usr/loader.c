#include <core/elf.h>
#include <drivers/fs.h>
#include <lib/string.h>

#define KERNMAX 0x00400000
#define USERSTACK (64 * 1024)
#define ARGMAX 16

extern u64 ram_max;

int load_segment(Elf32_Phdr* phdr, int fd) {
    u8* dest = (u8*)phdr->p_vaddr;
    if (lseek(fd, phdr->p_offset, SEEK_SET) < 0) {
        return -1;
    }

    if (phdr->p_memsz > phdr->p_filesz) {
        memset(dest, 0, phdr->p_memsz);
    }

    ssize nread = read(fd, dest, phdr->p_filesz);
    if (nread < 0 || (usize)nread < phdr->p_filesz) {
        return -1;
    }

    return 0;
}

int load_program(const char* path, char** argv) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    Elf32_Ehdr ehdr;
    ssize nread = read(fd, &ehdr, sizeof(ehdr));
    if (nread == -1 || (usize)nread < sizeof(ehdr)) {
        close(fd);
        return -1;
    }

    if (ehdr.e_ident[EI_MAG0]    != ELFMAG0     ||
        ehdr.e_ident[EI_MAG1]    != ELFMAG1     ||
        ehdr.e_ident[EI_MAG2]    != ELFMAG2     ||
        ehdr.e_ident[EI_MAG3]    != ELFMAG3     ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS32  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB) {
            close(fd);
            return -1;
    }

    if (ehdr.e_type    != ET_EXEC ||
        ehdr.e_machine != EM_386  ||
        ehdr.e_version != EV_CURRENT) {
            close(fd);
            return -1;
    }

    if (lseek(fd, ehdr.e_phoff, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    Elf32_Phdr phdrs[ehdr.e_phnum];
    u32 load_high = KERNMAX;
    u32 load_low = ram_max;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        ssize nread = read(fd, &phdrs[i], sizeof(Elf32_Phdr));
        if (nread == -1 || (usize)nread != ehdr.e_phentsize) {
            close(fd);
            return -1;
        }
        if (phdrs[i].p_vaddr < KERNMAX || (phdrs[i].p_vaddr + phdrs[i].p_memsz) > ram_max) {
            close(fd);
            return -1;
        }

        if (phdrs[i].p_vaddr > load_high) load_high = phdrs[i].p_vaddr;
        if (phdrs[i].p_vaddr < load_low)  load_low  = phdrs[i].p_vaddr;
    }

    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (load_segment(&phdrs[i], fd) < 0) {
                close(fd);
                return -1;
            }
        }
    }

    close(fd);

    u32 esp = load_high + USERSTACK;
    u32 esp0 = load_low + esp;
    u32 ucode = 0x1B;
    u32 udata = 0x23;

    u32 esp_cpy = esp;
    u32 esp0_cpy = esp0;

    int ac = 0;
    while (argv[ac] != NULL && ac < ARGMAX) {
        ac++;
    }

    u32 avaddrs[ARGMAX] = {0};

    for (int i = ac - 1; i >= 0; i--) {
        u32 len = strlen(argv[i]) + 1;
        esp_cpy -= len;
        esp0_cpy -= len;

        memcpy((void*)esp0_cpy, argv[i], len);
        avaddrs[i] = esp_cpy;
    }

    esp0_cpy &= ~3;
    esp_cpy &= ~3;

    esp0_cpy -= sizeof(u32);
    esp_cpy -= sizeof(u32);
    *(u32*)esp0_cpy = 0;

    for (int i = ac - 1; i >= 0; i--) {
        esp0_cpy -= sizeof(u32);
        esp_cpy -= sizeof(u32);
        *(u32*)esp0_cpy = avaddrs[i];
    }

    u32 virt_argv_ptr = esp_cpy;

    esp0_cpy -= sizeof(u32); esp_cpy -= sizeof(u32);
    *(u32*)esp0_cpy = virt_argv_ptr;

    esp0_cpy -= sizeof(u32); esp_cpy -= sizeof(u32);
    *(u32*)esp0_cpy = (u32)ac;

    asm volatile(
        "cli\n\t"
        
        "mov %0, %%ds\n\t"
        "mov %0, %%es\n\t"
        "mov %0, %%fs\n\t"
        "mov %0, %%gs\n\t"

        "pushl %0\n\t"
        "pushl %1\n\t"

        "pushfl\n\t"
        "popl %%eax\n\t"
        "orl $0x200, %%eax\n\t"
        "pushl %%eax\n\t"
        
        "pushl %2\n\t"
        "pushl %3\n\t"

        "iret\n\t"

        :: "r"(udata), "r"(esp_cpy), 
           "r"(ucode), "r"(ehdr.e_entry)
        : "eax", "memory"
    );

    return -1;    
}