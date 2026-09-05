#pragma once
#include <sys/types.h>
// sometime should implement more stuff
// including GNU extensions (LSB Core Generic)
// and ones in the new SysV ELF spec from Xinuos

typedef u64 Elf64_Addr;
typedef u64 Elf64_Off;
typedef u16 Elf64_Half;
typedef u32 Elf64_Word;
typedef s32 Elf64_Sword;
typedef u64 Elf64_Xword;
typedef s64 Elf64_Sxword;

#define EI_NIDENT 16

typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf64_Half      e_type;
    Elf64_Half      e_machine;
    Elf64_Word      e_version;
    Elf64_Addr      e_entry;
    Elf64_Off       e_phoff;
    Elf64_Off       e_shoff;
    Elf64_Word      e_flags;
    Elf64_Half      e_ehsize;
    Elf64_Half      e_phentsize;
    Elf64_Half      e_phnum;
    Elf64_Half      e_shentsize;
    Elf64_Half      e_shnum;
    Elf64_Half      e_shstrndx;
} Elf64_Ehdr;

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8
#define EI_PAD 9

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EM_X86_64 62

#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define EV_NONE 0
#define EV_CURRENT 1

#define SHN_UNDEF 0

typedef struct {
	Elf64_Word	p_type;
	Elf64_Word	p_flags;
	Elf64_Off	p_offset;
	Elf64_Addr	p_vaddr;
	Elf64_Addr	p_paddr;
	Elf64_Xword	p_filesz;
	Elf64_Xword	p_memsz;
	Elf64_Xword	p_align;
} Elf64_Phdr;

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 	7

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x3

typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

typedef struct {
	Elf64_Word	st_name;
	unsigned char st_info;
	unsigned char st_other;
	Elf64_Half	st_shndx;
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;
} Elf64_Sym;

// we should also add symbol visibility support
// sometime soon

#define STN_UNDEF 0
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC   2

// dynamic linking stuff

typedef struct {
	Elf64_Sxword	d_tag;
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
} Elf64_Rel;

typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
	Elf64_Sxword	r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i)   ((i) >> 32)
#define ELF64_R_TYPE(i)  ((i) & 0xFFFFFFFF)

// d_tag values

// dynld first pass
#define DT_HASH     4
#define DT_GNU_HASH 0x6ffffef5
#define DT_SYMTAB   6
#define DT_SYMENT  11
#define DT_STRTAB   5
#define DT_STRSZ   10

// dynld second pass
#define DT_NEEDED   1

#define DT_JMPREL  23
#define DT_PLTRELSZ 2
#define DT_PLTREL  20

#define DT_RELA     7
#define DT_RELASZ   8

#define DT_REL     17
#define DT_RELSZ   18

#define DT_INIT    12
#define DT_FINI    13
#define DT_INITARRAY 25
#define DT_FINIARRAY 26
#define DT_INITARRAYSZ 27
#define DT_FINIARRAYSZ 28
#define DT_PREINITARRAY 32
#define DT_PREINITARRAYSZ 33

#define DT_NULL     0

// reloc types (x86_64)
#define R_X86_64_NONE      0
#define R_X86_64_64        1
#define R_X86_64_COPY      5
#define R_X86_64_GLOB_DAT  6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE  8

// section types
#define SHT_NULL    0
#define SHT_SYMTAB  2
#define SHT_STRTAB  3
#define SHT_RELA    4
#define SHT_DYNSYM 11
#define SHT_DYNAMIC 6

#define AT_NULL    0
#define AT_PHDR    1
#define AT_PHNUM   2
#define AT_ENTRY   3
#define AT_IPHDRS  4
#define AT_IPHNUM  5
#define AT_STACK   6
#define AT_STACKSZ 7

// provided by ldso
#define AT_MMAPLOW  8
#define AT_MMAPHIGH 9

#define NAUXV 8
typedef struct {
    u64 type;
    u64 val;
} Elf64_Auxv;

u64 getauxval(u64 type);