#include "ldso.h"

HIDDEN NORETURN void ldso_start(void* rsp);
HIDDEN NORETURN HIDDEN void ldso_main(u64 ldso_base, u64 argc, char** argv, char** envp, Elf64_Auxv* auxv);

ASMFUNC NORETURN void _start(void) {
    asm volatile(
        "mov %%rsp, %%rdi\n\t"
        "and $-16, %%rsp\n\t"
        "jmp *%0"
        :: "c"(ldso_start)
        : "rsp", "memory"
    );
}

HIDDEN object_t ldso_obj;

// elf needs to parse its own DT_RELA and DT_REL first
HIDDEN void ldso_start(void* rsp) {
    const char ldso_startup_msg[] = "ldso\n";
    asm volatile(
        "mov $54, %%rax\n\t"
        "syscall\n\t"
        :
        : "D"(ldso_startup_msg), "S"((usize)5)
        : "rax", "rcx", "r11", "memory"
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

    object_t _ldso_obj = {
        "ldso",
        {0},
        0,
        OBJ_INITED,
        ldso_base,
        NULL,

        NULL,
        0,
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,

        NULL,
        0,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0
    };

    if (dyns_addr) {
        Elf64_Dyn* dyns = (Elf64_Dyn*)(ldso_base + dyns_addr);
        _ldso_obj.dynbase = dyns;
        
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
                case DT_SYMTAB: _ldso_obj.dynsym = (Elf64_Sym*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_PLTGOT: _ldso_obj.got = (u64*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_JMPREL: _ldso_obj.pltrel_base = (void*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_STRTAB: _ldso_obj.dynstr = (char*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_SYMENT: _ldso_obj.dynsymentsz = dyns->d_un.d_val; break;
                case DT_HASH: _ldso_obj.htab = (u32*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_GNU_HASH: _ldso_obj.gnu_htab = (u32*)(ldso_base + dyns->d_un.d_ptr); break;
                case DT_PLTRELSZ: _ldso_obj.pltrel_sz = dyns->d_un.d_val; break;
                case DT_PLTREL: _ldso_obj.pltrel_type = dyns->d_un.d_val; break;
            }
            dyns++;
        }

        if (_ldso_obj.pltrel_base - ldso_base) {
            if (_ldso_obj.pltrel_type == DT_RELA) {
                Elf64_Rela* relas = (Elf64_Rela*)_ldso_obj.pltrel_base;
                usize nrelas = _ldso_obj.pltrel_sz / sizeof(Elf64_Rela);
                for (usize i = 0; i < nrelas; i++) {
                    Elf64_Rela* rela = &relas[i];
                    if (ELF64_R_TYPE(rela->r_info) == R_X86_64_GLOB_DAT || ELF64_R_TYPE(rela->r_info) == R_X86_64_JUMP_SLOT) {
                        Elf64_Sym* sym = &_ldso_obj.dynsym[ELF64_R_SYM(rela->r_info)];
                        *((u64*)(ldso_base + rela->r_offset)) = ldso_base + sym->st_value + rela->r_addend;
                    }
                }
            } else if (_ldso_obj.pltrel_type == DT_REL) {
                Elf64_Rel* rels = (Elf64_Rel*)_ldso_obj.pltrel_base;
                usize nrels = _ldso_obj.pltrel_sz / sizeof(Elf64_Rel);
                for (usize i = 0; i < nrels; i++) {
                    Elf64_Rel* rel = &rels[i];
                    if (ELF64_R_TYPE(rel->r_info) == R_X86_64_GLOB_DAT || ELF64_R_TYPE(rel->r_info) == R_X86_64_JUMP_SLOT) {
                        Elf64_Sym* sym = &_ldso_obj.dynsym[ELF64_R_SYM(rel->r_info)];
                        u64 tgt = ldso_base + rel->r_offset;
                        u64 addend = *(u64*)tgt;
                        *((u64*)tgt) = ldso_base + sym->st_value + addend;
                    }
                }
            }
        }

        if (rela_addr && rela_sz != 0) {
            Elf64_Rela* relas = (Elf64_Rela*)(ldso_base + rela_addr);
            usize nrelas = rela_sz / sizeof(Elf64_Rela);
            for (usize i = 0; i < nrelas; i++) {
                Elf64_Rela* rela = &relas[i];
                if (ELF64_R_TYPE(rela->r_info) == R_X86_64_RELATIVE) {
                    *((u64*)(ldso_base + rela->r_offset)) = ldso_base + rela->r_addend;
                } else if (ELF64_R_TYPE(rela->r_info) == R_X86_64_GLOB_DAT || ELF64_R_TYPE(rela->r_info) == R_X86_64_JUMP_SLOT) {
                    Elf64_Sym* sym = &_ldso_obj.dynsym[ELF64_R_SYM(rela->r_info)];
                    *((u64*)(ldso_base + rela->r_offset)) = ldso_base + sym->st_value + rela->r_addend;
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
                } else if (ELF64_R_TYPE(rel->r_info) == R_X86_64_GLOB_DAT || ELF64_R_TYPE(rel->r_info) == R_X86_64_JUMP_SLOT) {
                    Elf64_Sym* sym = &_ldso_obj.dynsym[ELF64_R_SYM(rel->r_info)];
                    u64 tgt = ldso_base + rel->r_offset;
                    u64 addend = *(u64*)tgt;
                    *((u64*)tgt) = ldso_base + sym->st_value + addend;
                }
            }
        }
    }

    memcpy(&ldso_obj, &_ldso_obj, sizeof(ldso_obj));
    ldso_main(ldso_base, argc, argv, envp, actual_auxv);
}