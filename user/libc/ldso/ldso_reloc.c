#include "ldso.h"

HIDDEN int apply_rel(Elf64_Rel* rel, object_t* obj) {
    u64 tgt = obj->base + rel->r_offset;
    u64 addend = *(u64*)tgt;
    u64 v2r = 0;

    switch (ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_RELATIVE: {
            v2r = obj->base + addend;
            break;
        }
        case R_X86_64_JUMP_SLOT: {
            *((u64*)tgt) += obj->base;
            return 0;
        }
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
        case R_X86_64_64:
        case R_X86_64_JUMP_SLOT: {
            *((u64*)tgt) += obj->base;
            return 0;
        }
        case R_X86_64_GLOB_DAT: {
            Elf64_Sym* sym = &obj->dynsym[ELF64_R_SYM(rela->r_info)];
            u64 symaddr = 0;
            const char* sname = obj->dynstr + sym->st_name;

            if (sym->st_shndx != SHN_UNDEF) {
                symaddr = obj->base + sym->st_value;
            } else {
                symaddr = locate_extern(sname, NULL);
                if (!symaddr) {
                    ldso_print("[ldso] symbol not found: ");
                    ldso_print(sname);
                    ldso_print("\n");
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
            return 0;
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