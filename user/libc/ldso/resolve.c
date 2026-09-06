#include "ldso.h"

u64 ldso_resvmain(u64 obj_ident, u64 pltidx);
ASMFUNC void ldso_resolve() {
    asm volatile(
        "pop %%r12\n\t"
        "pop %%r13\n\t"
        "push %%rax\n\t"
        "push %%rcx\n\t"
        "push %%rdx\n\t"
        "push %%rsi\n\t"
        "push %%rdi\n\t"
        "push %%r8\n\t"
        "push %%r9\n\t"
        "push %%r10\n\t"
        "push %%r11\n\t"
        "mov %%r12, %%rdi\n\t"
        "mov %%r13, %%rsi\n\t"
        "call *%0\n\t"
        "mov %%rax, %%rbx\n\t"
        "pop %%r11\n\t"
        "pop %%r10\n\t"
        "pop %%r9\n\t"
        "pop %%r8\n\t"
        "pop %%rdi\n\t"
        "pop %%rsi\n\t"
        "pop %%rdx\n\t"
        "pop %%rcx\n\t"
        "pop %%rax\n\t"
        "jmp *%%rbx"
        :: "r"(ldso_resvmain)
        : "memory", "r12", "r13", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "rbx"
    );
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

HIDDEN u64 locate_hashsym(object_t* obj, const char* name, usize* sz) {
    if (!obj->htab) return STN_UNDEF;
    u32 h = elf_hash(name);

    u32 nbuckets = *obj->htab;
    u32* buckets = obj->htab + 2;
    u32* chains = buckets + nbuckets;

    u32 i = buckets[h % nbuckets];
    while (i != STN_UNDEF) {
        Elf64_Sym* sym = &obj->dynsym[i];
        if (sym->st_shndx != SHN_UNDEF && streq(obj->dynstr + sym->st_name, name)) {
            if (sz) *sz = sym->st_size;
            return obj->base + sym->st_value;
        }
        i = chains[i];
    }
    return STN_UNDEF;
}

HIDDEN u32 gnu_hash(const char* s) {
    u32 h = 5381;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        h = (h << 5) + h + *p;
    }
    return h;
}

HIDDEN u64 locate_gnuhashsym(object_t* obj, const char* name, usize* sz) {
    if (!obj->gnu_htab) return STN_UNDEF;
    u32* hdr = obj->gnu_htab;
    u32 nbuckets = hdr[0];
    u32 symoffset = hdr[1];
    u32 bloom_size = hdr[2];
    u32 bloom_shift = hdr[3];

    u64* bloom = (u64*)(hdr + 4);
    u32* buckets = (u32*)(bloom + bloom_size);
    u32* chains = buckets + nbuckets;

    u32 h = gnu_hash(name);

    u64 word = bloom[(h / 64) % bloom_size];
    u64 mask = (1ULL << (h % 64)) | (1ULL << ((h >> bloom_shift) % 64));
    if ((word & mask) != mask) {
        return STN_UNDEF;
    }

    u32 symix = buckets[h % nbuckets];
    if (symix < symoffset) {
        return STN_UNDEF;
    }

    for (;; symix++) {
        Elf64_Sym* sym = &obj->dynsym[symix];
        u32 chain = chains[symix - symoffset];
        if (((h ^ chain) >> 1) == 0) {
            if (sym->st_shndx != SHN_UNDEF && streq(obj->dynstr + sym->st_name, name)) {
                if (sz) *sz = sym->st_size;
                return obj->base + sym->st_value;
            }
        }
        if (chain & 1) break;
    }
    return STN_UNDEF;
}

HIDDEN u64 locate_extern(const char* name, usize* sz) {
    u64 i = STN_UNDEF;
    if (ldso_obj.gnu_htab) {
        i = locate_gnuhashsym(&ldso_obj, name, sz);
    }

    if (i == STN_UNDEF && ldso_obj.htab) {
        i = locate_hashsym(&ldso_obj, name, sz);
    }

    if (i != STN_UNDEF) {
        return i;
    }

    for (usize l = 0; l < nloaded; l++) {
        object_t* obj = &objects[l];
        u64 i = STN_UNDEF;
        if (obj->gnu_htab) {
            i = locate_gnuhashsym(obj, name, sz);
        }
        if (i == STN_UNDEF && obj->htab) {
            i = locate_hashsym(obj, name, sz);
        }
        if (i != STN_UNDEF) {
            return i;
        }
    }
    return 0;
}

u64 ldso_resvmain(u64 obj_ident, u64 pltidx) {
    object_t* obj = (object_t*)obj_ident;

    Elf64_Sym* sym = NULL;
    u64 addend = 0;
    u64 tgt = 0;

    // this is garunteed to be R_X86_64_JUMP_SLOT so yeah
    if (obj->pltrel_type == DT_REL) {
        Elf64_Rel* rel = &((Elf64_Rel*)obj->pltrel_base)[pltidx];
        tgt = obj->base + rel->r_offset;
        addend = *(u64*)tgt;
        sym = &obj->dynsym[ELF64_R_SYM(rel->r_info)];
    } else if (obj->pltrel_type == DT_RELA) {
        Elf64_Rela* rela = &((Elf64_Rela*)obj->pltrel_base)[pltidx];
        tgt = obj->base + rela->r_offset;
        sym = &obj->dynsym[ELF64_R_SYM(rela->r_info)];
        addend = rela->r_addend;
    } else {
        ldso_print("Object has bad GOT relocation table. Aborting now\n");
        ldso_exit(1);
    }

    u64 symaddr = 0;
    const char* name = obj->dynstr + sym->st_name;
    if (sym->st_shndx != SHN_UNDEF) {
        symaddr = obj->base + sym->st_value;
    } else {
        symaddr = locate_extern(name, NULL);
        if (!symaddr) {
            ldso_print("Unable to resolve symbol ");
            ldso_print(name);
            ldso_print(". Aborting now\n");
            ldso_exit(1);
        }
    }
    
    *((u64*)tgt) = symaddr + addend;
    return symaddr + addend;
}